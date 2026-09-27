// src/eow.c
#include "eow.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Kennung der Volume-Variante mit EOW im BitLocker-Volume-Kopf (Offset 0xA0),
// GUID 92A84D3B-DD80-4D0E-9E4E-B1E3284EAED8 in der Byte-Reihenfolge auf dem Datenträger.
static const unsigned char EOW_VOLUME_ID[16] = {0x3b, 0x4d, 0xa8, 0x92, 0x80, 0xdd, 0x0e, 0x4d,
                                                0x9e, 0x4e, 0xb1, 0xe3, 0x28, 0x4e, 0xae, 0xd8};

#define MAX_DESCRIPTOR_SIZE 65536u
#define MAX_RECORD_SIZE 65536u
#define MAX_BLOCK_MAPS 4096u
#define METADATA_ALIGN 4096u

uint32_t eow_crc32(const unsigned char *data, size_t len)
{
    static uint32_t table[256];
    static int ready = 0;
    if (!ready)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t le64(const unsigned char *p) { return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32); }

static int read_at(int fd, uint64_t offset, unsigned char *buf, size_t len)
{
    while (len > 0)
    {
        ssize_t n = pread(fd, buf, len, (off_t)offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return 0;
        buf += n;
        len -= (size_t)n;
        offset += (uint64_t)n;
    }
    return 1;
}

// Prüfsumme über len Byte, wobei das Prüfsummenfeld selbst als 0 zählt.
static int checksum_ok(unsigned char *buf, size_t len, size_t field)
{
    uint32_t stored = le32(buf + field);
    unsigned char saved[4];
    memcpy(saved, buf + field, 4);
    memset(buf + field, 0, 4);
    uint32_t actual = eow_crc32(buf, len);
    memcpy(buf + field, saved, 4);
    return stored == actual;
}

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

typedef struct
{
    EowRange *items;
    size_t count, cap;
} RangeList;

static int push(RangeList *l, uint64_t offset, uint64_t length, EowKind kind)
{
    if (length == 0)
        return 1;
    // Unmittelbar anschließende Bereiche gleicher Art zusammenfassen
    if (l->count && l->items[l->count - 1].kind == kind &&
        l->items[l->count - 1].offset + l->items[l->count - 1].length == offset)
    {
        l->items[l->count - 1].length += length;
        return 1;
    }
    if (l->count == l->cap)
    {
        size_t cap = l->cap ? l->cap * 2 : 64;
        EowRange *n = realloc(l->items, cap * sizeof(*n));
        if (!n)
            return 0;
        l->items = n;
        l->cap = cap;
    }
    l->items[l->count++] = (EowRange){offset, length, kind};
    return 1;
}

static int by_offset(const void *a, const void *b)
{
    const EowRange *x = a, *y = b;
    return x->offset < y->offset ? -1 : x->offset > y->offset;
}

// Sortiert und vereinigt überlappende Bereiche einer Art.
static int normalize(RangeList *l)
{
    if (l->count < 2)
        return 1;
    qsort(l->items, l->count, sizeof(EowRange), by_offset);
    size_t w = 0;
    for (size_t r = 1; r < l->count; r++)
    {
        EowRange *last = &l->items[w];
        uint64_t end = last->offset + last->length;
        if (l->items[r].offset <= end)
        {
            uint64_t e2 = l->items[r].offset + l->items[r].length;
            if (e2 > end)
                last->length = e2 - last->offset;
        }
        else
            l->items[++w] = l->items[r];
    }
    l->count = w + 1;
    return 1;
}

static int add_clamped(RangeList *l, uint64_t offset, uint64_t length, uint64_t limit, EowKind kind)
{
    if (offset >= limit || length == 0)
        return 1;
    if (length > limit - offset)
        length = limit - offset;
    return push(l, offset, length, kind);
}

#define FAIL(...)                                  \
    do                                             \
    {                                              \
        snprintf(err, err_len, __VA_ARGS__);       \
        goto fail;                                 \
    } while (0)

int eow_read(int fd, uint64_t part_offset, uint64_t part_bytes, EowMap *map, char *err, size_t err_len)
{
    memset(map, 0, sizeof(*map));
    unsigned char head[512];
    unsigned char *desc = NULL, *sector = NULL, *rec = NULL, *best = NULL;
    RangeList plain = {0}, meta = {0}, out = {0};
    if (err_len)
        err[0] = '\0';

    if (!read_at(fd, part_offset, head, sizeof(head)))
    {
        snprintf(err, err_len, "Volume-Kopf nicht lesbar");
        return -1;
    }
    if (memcmp(head + 3, "-FVE-FS-", 8) != 0 || memcmp(head + 0xA0, EOW_VOLUME_ID, 16) != 0)
        return 0;

    // Zwei Kopien des Deskriptors; die erste mit gültiger Prüfsumme gilt.
    uint64_t desc_offsets[2] = {le64(head + 0xC8), le64(head + 0xD0)};
    uint32_t desc_size = 0;
    for (int i = 0; i < 2 && !desc; i++)
    {
        uint64_t o = desc_offsets[i];
        unsigned char h[56];
        if (o == 0 || o >= part_bytes || !read_at(fd, part_offset + o, h, sizeof(h)))
            continue;
        if (memcmp(h, "FVE-EOW\0", 8) != 0 || le16(h + 8) != 56)
            continue;
        uint32_t size = le16(h + 10);
        if (size < 56 || size > MAX_DESCRIPTOR_SIZE || o + size > part_bytes)
            continue;
        unsigned char *d = malloc(size);
        if (!d)
            FAIL("kein Speicher");
        if (read_at(fd, part_offset + o, d, size) && checksum_ok(d, size, 36))
        {
            desc = d;
            desc_size = size;
            map->descriptor_offset = o;
        }
        else
            free(d);
    }
    if (!desc)
        FAIL("EOW-Kennung im Volume-Kopf, aber kein EOW-Deskriptor mit gültiger Prüfsumme");

    uint32_t phys = le32(desc + 16);
    uint32_t rbs = le32(desc + 20);
    uint32_t log_size = le32(desc + 24);
    uint32_t nmaps = le32(desc + 32);
    if (phys < 512 || phys > 65536 || (phys & (phys - 1)) != 0)
        FAIL("EOW-Deskriptor: ungültige Sektorgröße %" PRIu32, phys);
    if (rbs == 0 || rbs % 512 != 0 || rbs > (1u << 30))
        FAIL("EOW-Deskriptor: ungültige Blockgröße %" PRIu32, rbs);
    if (nmaps == 0 || nmaps > MAX_BLOCK_MAPS || 56 + (uint64_t)nmaps * 8 > desc_size)
        FAIL("EOW-Deskriptor: ungültige Zahl von Block-Maps %" PRIu32, nmaps);
    map->relocation_block_size = rbs;
    map->maps = calloc(nmaps, sizeof(EowMapInfo));
    sector = malloc(phys);
    rec = malloc(MAX_RECORD_SIZE);
    best = malloc(MAX_RECORD_SIZE);
    if (!map->maps || !sector || !rec || !best)
        FAIL("kein Speicher");

    for (int i = 0; i < 2; i++)
        if (desc_offsets[i] && !add_clamped(&meta, desc_offsets[i], METADATA_ALIGN, part_bytes, EOW_META))
            FAIL("kein Speicher");

    for (uint32_t m = 0; m < nmaps; m++)
    {
        uint64_t bmo = le64(desc + 56 + 8 * (size_t)m);
        if (bmo >= part_bytes || part_bytes - bmo < phys || !read_at(fd, part_offset + bmo, sector, phys))
            FAIL("Block-Map %" PRIu32 " bei 0x%" PRIx64 " nicht lesbar", m, bmo);
        if (memcmp(sector, "FVE-EOWBM\0", 10) != 0 || le16(sector + 10) != 60 || !checksum_ok(sector, phys, 56))
            FAIL("Block-Map %" PRIu32 " bei 0x%" PRIx64 ": Signatur oder Prüfsumme ungültig", m, bmo);
        uint32_t bm_size = le32(sector + 12);
        uint64_t region = le64(sector + 20), region_size = le64(sector + 28), log_off = le64(sector + 36);
        uint32_t rec_off[2] = {le32(sector + 44), le32(sector + 48)};
        uint32_t rec_size = le32(sector + 52);
        if (region > part_bytes || region_size > part_bytes - region)
            FAIL("Block-Map %" PRIu32 ": Bereich liegt außerhalb des Volumes", m);
        if (rec_size < 40 || rec_size > MAX_RECORD_SIZE)
            FAIL("Block-Map %" PRIu32 ": ungültige Record-Größe %" PRIu32, m, rec_size);

        // Zwei Records; der gültige mit der höchsten Sequenznummer ist maßgeblich.
        int have = 0;
        uint64_t best_seq = 0;
        for (int r = 0; r < 2; r++)
        {
            uint64_t ro = bmo + rec_off[r];
            if (rec_off[r] == 0 || ro >= part_bytes || part_bytes - ro < rec_size ||
                !read_at(fd, part_offset + ro, rec, rec_size))
                continue;
            if (memcmp(rec, "FVE-EOWBR\0", 10) != 0 || le16(rec + 10) != 36 || !checksum_ok(rec, rec_size, 32))
                continue;
            uint64_t seq = le64(rec + 20);
            if (!have || seq > best_seq)
            {
                memcpy(best, rec, rec_size);
                best_seq = seq;
                have = 1;
            }
        }
        if (!have)
            FAIL("Block-Map %" PRIu32 ": kein Block-Record mit gültiger Prüfsumme", m);

        uint32_t bits = le32(best + 16);
        if (((uint64_t)bits + 7) / 8 > rec_size - 36)
            FAIL("Block-Map %" PRIu32 ": Bitmap größer als der Record", m);
        if ((uint64_t)bits * rbs < region_size)
            FAIL("Block-Map %" PRIu32 ": Bitmap deckt den Bereich nicht ab", m);
        const unsigned char *bitmap = best + 36;

        EowMapInfo *info = &map->maps[map->map_count++];
        *info = (EowMapInfo){le32(sector + 16), bmo, region, region_size, best_seq, bits, 0};
        for (uint32_t b = 0; b < bits; b++)
        {
            uint64_t start = (uint64_t)b * rbs;
            if (start >= region_size)
                break;
            uint64_t len = region_size - start < rbs ? region_size - start : rbs;
            if (bitmap[b >> 3] & (1u << (b & 7)))
                info->bits_set++;
            else if (!add_clamped(&plain, region + start, len, part_bytes, EOW_PLAIN))
                FAIL("kein Speicher");
        }

        // Verwaltungsbereiche dieser Block-Map
        if (!add_clamped(&meta, bmo, align_up(bm_size ? bm_size : phys, METADATA_ALIGN), part_bytes, EOW_META))
            FAIL("kein Speicher");
        if (log_off && !add_clamped(&meta, log_off, align_up(log_size, METADATA_ALIGN), part_bytes, EOW_META))
            FAIL("kein Speicher");
    }

    // Verwaltungsdaten haben Vorrang vor Klartextbereichen
    normalize(&plain);
    normalize(&meta);
    size_t mi = 0;
    for (size_t p = 0; p < plain.count; p++)
    {
        uint64_t s = plain.items[p].offset, e = s + plain.items[p].length;
        while (mi < meta.count && meta.items[mi].offset + meta.items[mi].length <= s)
            mi++;
        for (size_t k = mi; k < meta.count && meta.items[k].offset < e && s < e; k++)
        {
            uint64_t ms = meta.items[k].offset, me = ms + meta.items[k].length;
            if (ms > s && !push(&out, s, ms - s, EOW_PLAIN))
                FAIL("kein Speicher");
            if (me > s)
                s = me;
        }
        if (s < e && !push(&out, s, e - s, EOW_PLAIN))
            FAIL("kein Speicher");
    }
    for (size_t k = 0; k < meta.count; k++)
        if (!push(&out, meta.items[k].offset, meta.items[k].length, EOW_META))
            FAIL("kein Speicher");
    if (out.count > 1)
        qsort(out.items, out.count, sizeof(EowRange), by_offset);
    for (size_t k = 0; k < out.count; k++)
    {
        if (out.items[k].kind == EOW_PLAIN)
            map->plain_bytes += out.items[k].length;
        else
            map->meta_bytes += out.items[k].length;
    }
    map->ranges = out.items;
    map->range_count = out.count;

    free(desc);
    free(sector);
    free(rec);
    free(best);
    free(plain.items);
    free(meta.items);
    return 1;

fail:
    free(desc);
    free(sector);
    free(rec);
    free(best);
    free(plain.items);
    free(meta.items);
    free(out.items);
    eow_free(map);
    return -1;
}

void eow_free(EowMap *map)
{
    free(map->maps);
    free(map->ranges);
    memset(map, 0, sizeof(*map));
}

int eow_write_report(const EowMap *map, const char *path)
{
    FILE *f = fopen(path, "wx");
    if (!f)
        return 0;
    fprintf(f, "BitLocker Encrypt-on-Write (EOW)\n");
    fprintf(f, "Quelle der Formatbeschreibung: libbde-Dokumentation (libyal), BDE format, EOW-Abschnitte\n\n");
    fprintf(f, "EOW-Deskriptor (volume-relativ): 0x%" PRIx64 "\n", map->descriptor_offset);
    fprintf(f, "Blockgröße je Bit: %" PRIu32 " Byte\n", map->relocation_block_size);
    fprintf(f, "Block-Maps: %zu\n", map->map_count);
    for (size_t i = 0; i < map->map_count; i++)
    {
        const EowMapInfo *m = &map->maps[i];
        fprintf(f,
                "  Map %" PRIu32 " bei 0x%" PRIx64 ": Bereich 0x%" PRIx64 " + 0x%" PRIx64 ", Sequenz %" PRIu64
                ", %" PRIu32 " Bits, davon %" PRIu32 " verschlüsselt\n",
                m->index, m->block_map_offset, m->region_offset, m->region_size, m->sequence, m->bits,
                m->bits_set);
    }
    fprintf(f, "\nUnverschlüsselt aus dem Original übernommen: %" PRIu64 " Byte\n", map->plain_bytes);
    fprintf(f, "EOW-Verwaltungsdaten mit Nullen ausgegeben: %" PRIu64 " Byte\n", map->meta_bytes);
    fprintf(f, "Alle übrigen Bereiche: entschlüsselte Ausgabe von dislocker\n\n");
    fprintf(f, "Bereiche (volume-relativ, Beginn, Länge, Quelle):\n");
    for (size_t i = 0; i < map->range_count; i++)
        fprintf(f, "0x%012" PRIx64 " 0x%012" PRIx64 " %s\n", map->ranges[i].offset, map->ranges[i].length,
                map->ranges[i].kind == EOW_PLAIN ? "original" : "nullen");
    return fclose(f) == 0;
}
