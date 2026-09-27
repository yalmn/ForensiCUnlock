// tests/unit_tests.c
// Unit-Tests für die einzelnen Module. Aufruf über "make test" (Linux, als root).
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dislocker_runner.h"
#include "eow.h"
#include "exec_utils.h"
#include "image_converter.h"
#include "image_merger.h"
#include "ova_mounter.h"
#include "partition_parser.h"

static int failures = 0;
static int checks = 0;
static FILE *report; // Ausgabe der Testergebnisse, die Meldungen der Module landen in /dev/null
static char tmp_root[64];

#define CHECK(cond)                                                        \
    do                                                                     \
    {                                                                      \
        checks++;                                                          \
        if (!(cond))                                                       \
        {                                                                  \
            failures++;                                                    \
            fprintf(report, "FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                  \
    } while (0)

static void tmp_path(char *out, size_t len, const char *name)
{
    if (snprintf(out, len, "%s/%s", tmp_root, name) >= (int)len)
        exit(2);
}

static void write_file(const char *path, const unsigned char *data, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write(fd, data, len) != (ssize_t)len)
    {
        perror(path);
        exit(2);
    }
    close(fd);
}

static void touch(const char *path)
{
    write_file(path, (const unsigned char *)"x", 1);
}

static unsigned char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(*len ? *len : 1);
    if (fread(buf, 1, *len, f) != *len)
    {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

// Befüllt einen Puffer mit einem wiedererkennbaren Muster
static void fill_pattern(unsigned char *buf, size_t len, unsigned char seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (unsigned char)(seed + i * 7 + (i >> 9));
}

/* exec_utils */

static void test_run_cmd(void)
{
    char *ok[] = {"true", NULL};
    char *fail[] = {"false", NULL};
    char *missing[] = {"gibt-es-nicht-4711", NULL};
    CHECK(run_cmd(ok) == 1);
    CHECK(run_cmd(fail) == 0);
    CHECK(run_cmd(missing) == 0);

    // Sonderzeichen werden ohne Shell unverändert übergeben
    char special[] = "a b 'c' \"d\" $(e) ;f";
    char *echo[] = {"printf", "%s", special, NULL};
    pid_t pid;
    FILE *fp = run_cmd_read(echo, &pid);
    CHECK(fp != NULL);
    char buf[128] = {0};
    CHECK(fp && fgets(buf, sizeof(buf), fp) != NULL);
    CHECK(strcmp(buf, special) == 0);
    CHECK(fp && close_cmd_read(fp, pid) == 1);

    char *sh_fail[] = {"sh", "-c", "echo x; exit 3", NULL};
    fp = run_cmd_read(sh_fail, &pid);
    CHECK(fp != NULL);
    while (fp && fgets(buf, sizeof(buf), fp))
        ;
    CHECK(fp && close_cmd_read(fp, pid) == 0);
}

static void test_make_dir(void)
{
    char path[PATH_MAX];
    tmp_path(path, sizeof(path), "a/b c/d'e");
    CHECK(make_dir(path) == 1);
    struct stat st;
    CHECK(stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(make_dir(path) == 1); // existiert schon

    tmp_path(path, sizeof(path), "datei");
    touch(path);
    CHECK(make_dir(path) == 0); // Datei statt Verzeichnis
}

static void test_mountpoint(void)
{
    char path[PATH_MAX];
    CHECK(is_mountpoint("/") == 1);
    CHECK(is_mountpoint("/proc") == 1);
    tmp_path(path, sizeof(path), "a");
    CHECK(is_mountpoint(path) == 0);
    tmp_path(path, sizeof(path), "gibt-es-nicht");
    CHECK(is_mountpoint(path) == 0);
    CHECK(unmount_fuse(path) == 1); // nichts eingehängt

    // Echter Mount (tmpfs) muss erkannt und ausgehängt werden
    tmp_path(path, sizeof(path), "mnt");
    CHECK(make_dir(path));
    char *mount_argv[] = {"mount", "-t", "tmpfs", "tmpfs", path, NULL};
    if (run_cmd(mount_argv))
    {
        CHECK(is_mountpoint(path) == 1);
        CHECK(unmount_fuse(path) == 1);
        CHECK(is_mountpoint(path) == 0);
    }
    else
    {
        fprintf(report, "HINWEIS: tmpfs-Mount nicht möglich, Mount-Test übersprungen (root/--privileged nötig)\n");
    }
}

static void test_program_in_path(void)
{
    // Ein sicher vorhandenes Programm wird gefunden, ein erfundenes nicht.
    CHECK(program_in_path("sh") == 1);
    CHECK(program_in_path("dieses-programm-gibt-es-ganz-sicher-nicht-42") == 0);
    // Ein absoluter Pfad ist kein PATH-Eintrag und wird hier nicht aufgelöst.
    CHECK(program_in_path("/bin/sh") == 0);
}

/* image_converter */

static void test_is_ewf_path(void)
{
    CHECK(is_ewf_path("/cases/disk.E01") == 1);
    CHECK(is_ewf_path("disk.e01") == 1);
    CHECK(is_ewf_path("disk.E02") == 1);
    CHECK(is_ewf_path("disk.EAA") == 0); // nie das erste Segment
    CHECK(is_ewf_path("disk.Ex01") == 1);
    CHECK(is_ewf_path("disk.ewf") == 1);
    CHECK(is_ewf_path("disk.EWF") == 1);
    CHECK(is_ewf_path("disk.dd") == 0);
    CHECK(is_ewf_path("disk.raw") == 0);
    CHECK(is_ewf_path("disk.E0A") == 0);
    CHECK(is_ewf_path("disk.exe") == 0);
    CHECK(is_ewf_path("/dev/sdb") == 0);
    CHECK(is_ewf_path("/cases.E01/disk") == 0);
    CHECK(is_ewf_path(".E01") == 0);
}

static void test_is_ova_path(void)
{
    CHECK(is_ova_path("/cases/vm.ova") == 1);
    CHECK(is_ova_path("vm.OVA") == 1);
    CHECK(is_ova_path("vm.Ova") == 1);
    CHECK(is_ova_path("disk.ovf") == 0); // nur .ova, nicht der reine Deskriptor
    CHECK(is_ova_path("disk.vmdk") == 0);
    CHECK(is_ova_path("disk.dd") == 0);
    CHECK(is_ova_path("/dev/sdb") == 0);
    CHECK(is_ova_path("/cases.ova/disk") == 0);
    CHECK(is_ova_path(".ova") == 0);
    CHECK(is_ova_path("ova") == 0);
}

static void check_ext(const char *first, int n, const char *expected)
{
    char out[8] = {0};
    int ok = ewf_segment_ext(first, n, out, sizeof(out));
    CHECK(ok == 1);
    CHECK(strcmp(out, expected) == 0);
    if (!ok || strcmp(out, expected) != 0)
        fprintf(report, "  ewf_segment_ext(%s, %d) = '%s', erwartet '%s'\n", first, n, out, expected);
}

static void test_ewf_segment_ext(void)
{
    check_ext("E01", 1, "E01");
    check_ext("E01", 9, "E09");
    check_ext("E01", 10, "E10");
    check_ext("E01", 99, "E99");
    check_ext("E01", 100, "EAA");
    check_ext("E01", 101, "EAB");
    check_ext("E01", 125, "EAZ");
    check_ext("E01", 126, "EBA");
    check_ext("E01", 775, "EZZ");
    check_ext("E01", 776, "FAA");
    check_ext("e01", 2, "e02");
    check_ext("e01", 100, "eaa");
    check_ext("Ex01", 2, "Ex02");
    check_ext("Ex01", 100, "ExAA");

    char out[8];
    CHECK(ewf_segment_ext("E01", 0, out, sizeof(out)) == 0);
    CHECK(ewf_segment_ext("dd", 1, out, sizeof(out)) == 0);
    CHECK(ewf_segment_ext("E01", 1, out, 3) == 0);
    CHECK(ewf_segment_ext("Ex01", 100 + 676, out, sizeof(out)) == 0);
}

static void test_check_ewf_segments(void)
{
    char dir[128], path[PATH_MAX];
    int count = -1;

    tmp_path(dir, sizeof(dir), "ewf ok");
    make_dir(dir);
    for (int i = 1; i <= 3; i++)
    {
        snprintf(path, sizeof(path), "%s/fall.E%02d", dir, i);
        touch(path);
    }
    snprintf(path, sizeof(path), "%s/anderer.E04", dir); // anderer Name zählt nicht
    touch(path);
    snprintf(path, sizeof(path), "%s/fall.E01", dir);
    CHECK(check_ewf_segments(path, &count) == 1);
    CHECK(count == 3);

    // Lücke: E02 fehlt
    snprintf(path, sizeof(path), "%s/fall.E02", dir);
    unlink(path);
    snprintf(path, sizeof(path), "%s/fall.E01", dir);
    CHECK(check_ewf_segments(path, &count) == 0);

    // Nicht das erste Segment angegeben
    snprintf(path, sizeof(path), "%s/fall.E03", dir);
    CHECK(check_ewf_segments(path, &count) == 0);

    // Datei fehlt
    snprintf(path, sizeof(path), "%s/fehlt.E01", dir);
    CHECK(check_ewf_segments(path, &count) == 0);

    // Übergang E99 -> EAA -> EAB
    tmp_path(dir, sizeof(dir), "ewf viele");
    make_dir(dir);
    for (int i = 1; i <= 101; i++)
    {
        char ext[8];
        ewf_segment_ext("E01", i, ext, sizeof(ext));
        snprintf(path, sizeof(path), "%s/gross.%s", dir, ext);
        touch(path);
    }
    snprintf(path, sizeof(path), "%s/gross.E01", dir);
    CHECK(check_ewf_segments(path, &count) == 1);
    CHECK(count == 101);

    // Einzelne .ewf-Datei und relative Pfade
    tmp_path(path, sizeof(path), "einzeln.ewf");
    touch(path);
    CHECK(check_ewf_segments(path, &count) == 1);
    CHECK(count == 1);

    char cwd[PATH_MAX];
    CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
    tmp_path(dir, sizeof(dir), "ewf ok");
    CHECK(chdir(dir) == 0);
    touch("rel.E01");
    touch("rel.E02");
    CHECK(check_ewf_segments("rel.E01", &count) == 1);
    CHECK(count == 2);
    CHECK(chdir(cwd) == 0);
}

/* partition_parser */

static void test_parse_mmls(void)
{
    uint32_t sector = 0;
    CHECK(parse_mmls_units("Units are in 512-byte sectors\n", &sector) == 1 && sector == 512);
    CHECK(parse_mmls_units("Units are in 4096-byte sectors\n", &sector) == 1 && sector == 4096);
    CHECK(parse_mmls_units("GUID Partition Table (EFI)\n", &sector) == 0);

    PartitionInfo p;
    CHECK(parse_mmls_entry("004:  000       0000002048   0000034815   0000032768   Basic data partition\n", &p) == 1);
    CHECK(p.slot == 4 && p.start == 2048 && p.length == 32768);
    CHECK(strcmp(p.description, "Basic data partition") == 0);

    CHECK(parse_mmls_entry("002:  000:000   0000002048   0000034815   0000032768   NTFS / exFAT (0x07)\n", &p) == 1);
    CHECK(p.slot == 2 && strcmp(p.description, "NTFS / exFAT (0x07)") == 0);

    // GPT ohne Partitionsnamen
    CHECK(parse_mmls_entry("005:  001       0000034816   0000067583   0000032768   \n", &p) == 1);
    CHECK(p.slot == 5 && p.start == 34816 && p.description[0] == '\0');

    CHECK(parse_mmls_entry("000:  Meta      0000000000   0000000000   0000000001   Safety Table\n", &p) == 0);
    CHECK(parse_mmls_entry("001:  -------   0000000000   0000002047   0000002048   Unallocated\n", &p) == 0);
    CHECK(parse_mmls_entry("      Slot      Start        End          Length       Description\n", &p) == 0);
    CHECK(parse_mmls_entry("Offset Sector: 0\n", &p) == 0);
    CHECK(parse_mmls_entry("\n", &p) == 0);
    CHECK(parse_mmls_entry("004:  000       0000002048   0000034815   0000000005   kaputt\n", &p) == 0);
}

static void test_bitlocker_signature(void)
{
    char path[PATH_MAX];
    unsigned char img[8192] = {0};

    // Windows 7+ Signatur bei Sektor 4 (Offset 2048)
    memcpy(img + 2048, "\xeb\x58\x90-FVE-FS-", 11);
    // BitLocker To Go: GUID bei Offset 0x1a8 in Sektor 8 (Offset 4096)
    static const unsigned char guid[16] = {0x3b, 0xd6, 0x67, 0x49, 0x29, 0x2e, 0xd8, 0x4a,
                                           0x83, 0x99, 0xf6, 0xa3, 0x39, 0xe3, 0xd0, 0x01};
    memcpy(img + 4096, "\xeb\x58\x90MSWIN4.1", 11);
    memcpy(img + 4096 + 0x1a8, guid, sizeof(guid));
    // NTFS ohne BitLocker
    memcpy(img + 6144, "\xeb\x52\x90NTFS    ", 11);

    tmp_path(path, sizeof(path), "signaturen.img");
    write_file(path, img, sizeof(img));

    CHECK(has_bitlocker_signature(path, 2048) == 1);
    CHECK(has_bitlocker_signature(path, 4096) == 1);
    CHECK(has_bitlocker_signature(path, 6144) == 0);
    CHECK(has_bitlocker_signature(path, 0) == 0);
    CHECK(has_bitlocker_signature(path, 8000) == 0); // zu wenig Daten
    CHECK(has_bitlocker_signature("/gibt/es/nicht", 0) == 0);
}

static void test_write_partition_info(void)
{
    char path[PATH_MAX];
    tmp_path(path, sizeof(path), "bdp.info");
    PartitionInfo p = {.slot = 4, .start = 256, .length = 1000, .sector_size = 4096};
    CHECK(write_partition_info(path, &p) == 1);

    size_t len;
    unsigned char *data = read_file(path, &len);
    CHECK(data != NULL);
    if (data)
    {
        const char *expected = "slot=4\nstart=256\nend=1255\nlength=1000\nsector_size=4096\noffset_bytes=1048576\n";
        CHECK(len == strlen(expected) && memcmp(data, expected, len) == 0);
        free(data);
    }
    CHECK(write_partition_info("/gibt/es/nicht/bdp.info", &p) == 0);
}

/* image_merger */

static void test_image_size(void)
{
    char path[PATH_MAX];
    uint64_t size = 0;
    tmp_path(path, sizeof(path), "größe.img");
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(fd >= 0 && ftruncate(fd, 123456789) == 0);
    close(fd);
    CHECK(get_image_size(path, &size) == 1 && size == 123456789);
    CHECK(get_image_size("/gibt/es/nicht", &size) == 0);
    CHECK(get_image_size("/dev/null", &size) == 0); // weder Datei noch Blockgerät

    // Blockgerät über ein Loop-Device
    char *losetup[] = {"losetup", "--find", "--show", path, NULL};
    pid_t pid;
    FILE *fp = run_cmd_read(losetup, &pid);
    char loopdev[64] = {0};
    int have_loop = fp && fgets(loopdev, sizeof(loopdev), fp) != NULL;
    if (fp)
        have_loop = close_cmd_read(fp, pid) && have_loop;
    if (have_loop)
    {
        loopdev[strcspn(loopdev, "\n")] = '\0';
        CHECK(get_image_size(loopdev, &size) == 1);
        CHECK(size == 123456789 / 512 * 512); // Loop-Devices runden auf volle Sektoren ab
        char *detach[] = {"losetup", "-d", loopdev, NULL};
        run_cmd(detach);
    }
    else
    {
        fprintf(report, "HINWEIS: Kein Loop-Device verfügbar, Blockgeräte-Test übersprungen\n");
    }
}

static void test_copy_range(void)
{
    char src[PATH_MAX], dst[PATH_MAX];
    size_t size = 3 * 1024 * 1024 + 777;
    unsigned char *data = malloc(size);
    fill_pattern(data, size, 1);
    memset(data + 1024 * 1024, 0, 1024 * 1024 + 100); // ergibt ab Offset 100 einen ganzen Nullblock
    tmp_path(src, sizeof(src), "quelle.bin");
    write_file(src, data, size);

    tmp_path(dst, sizeof(dst), "ziel.bin");
    int in = open(src, O_RDONLY);
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(copy_range(in, 100, size - 100, out, NULL) == 1);
    CHECK(ftruncate(out, (off_t)(size - 100)) == 0);
    close(out);

    size_t len;
    unsigned char *copy = read_file(dst, &len);
    CHECK(copy && len == size - 100 && memcmp(copy, data + 100, len) == 0);
    free(copy);

    // Nullblöcke werden nicht geschrieben, die Datei belegt weniger Platz
    struct stat st;
    CHECK(stat(dst, &st) == 0 && (uint64_t)st.st_blocks * 512 < (uint64_t)st.st_size);

    // Lesen über das Dateiende hinaus ist ein Fehler
    out = open(dst, O_WRONLY | O_TRUNC);
    CHECK(copy_range(in, size - 10, 20, out, NULL) == 0);

    // Abbruch-Flag wird beachtet
    volatile sig_atomic_t stop = 1;
    CHECK(copy_range(in, 0, size, out, &stop) == 0);
    close(out);
    close(in);
    free(data);
}

static void test_merge_image(void)
{
    enum { SECTOR = 512, START = 100, LENGTH = 300, TOTAL = 1000 };
    char raw[PATH_MAX], dec[PATH_MAX], merged[PATH_MAX], wrong[PATH_MAX];

    unsigned char *orig = malloc(TOTAL * SECTOR);
    unsigned char *plain = malloc(LENGTH * SECTOR);
    fill_pattern(orig, TOTAL * SECTOR, 3);
    fill_pattern(plain, LENGTH * SECTOR, 99);
    memset(orig + (TOTAL - 50) * SECTOR, 0, 50 * SECTOR); // Nullen am Ende testen ftruncate

    tmp_path(raw, sizeof(raw), "original.dd");
    tmp_path(dec, sizeof(dec), "dislocker-file");
    tmp_path(merged, sizeof(merged), "merged.dd");
    tmp_path(wrong, sizeof(wrong), "zu-kurz");
    write_file(raw, orig, TOTAL * SECTOR);
    write_file(dec, plain, LENGTH * SECTOR);
    write_file(wrong, plain, (LENGTH - 1) * SECTOR);

    PartitionInfo p = {.slot = 2, .start = START, .length = LENGTH, .sector_size = SECTOR};
    unlink(merged);
    CHECK(merge_image(raw, dec, &p, merged, NULL, NULL) == 1);

    size_t len;
    unsigned char *result = read_file(merged, &len);
    CHECK(result && len == TOTAL * SECTOR);
    if (result && len == TOTAL * SECTOR)
    {
        CHECK(memcmp(result, orig, START * SECTOR) == 0);
        CHECK(memcmp(result + START * SECTOR, plain, LENGTH * SECTOR) == 0);
        CHECK(memcmp(result + (START + LENGTH) * SECTOR, orig + (START + LENGTH) * SECTOR,
                     (TOTAL - START - LENGTH) * SECTOR) == 0);
    }
    free(result);

    // Bestehende Datei wird nicht überschrieben und bleibt unverändert
    CHECK(merge_image(raw, dec, &p, merged, NULL, NULL) == 0);
    result = read_file(merged, &len);
    CHECK(result && len == TOTAL * SECTOR);
    free(result);
    unlink(merged);

    // Partition am Anfang und am Ende des Images
    PartitionInfo first = {.slot = 1, .start = 0, .length = LENGTH, .sector_size = SECTOR};
    CHECK(merge_image(raw, dec, &first, merged, NULL, NULL) == 1);
    unlink(merged);
    PartitionInfo last = {.slot = 1, .start = TOTAL - LENGTH, .length = LENGTH, .sector_size = SECTOR};
    CHECK(merge_image(raw, dec, &last, merged, NULL, NULL) == 1);
    result = read_file(merged, &len);
    CHECK(result && len == TOTAL * SECTOR && memcmp(result + (TOTAL - LENGTH) * SECTOR, plain, LENGTH * SECTOR) == 0);
    free(result);
    unlink(merged);

    // 4K-Sektoren: gleiche Bytes, andere Einheit
    PartitionInfo k4 = {.slot = 2, .start = START * SECTOR / 4096 + 1, .length = LENGTH * SECTOR / 4096,
                        .sector_size = 4096};
    char dec4k[PATH_MAX];
    tmp_path(dec4k, sizeof(dec4k), "dislocker-4k");
    write_file(dec4k, plain, k4.length * 4096);
    CHECK(merge_image(raw, dec4k, &k4, merged, NULL, NULL) == 1);
    result = read_file(merged, &len);
    CHECK(result && len == TOTAL * SECTOR &&
          memcmp(result + k4.start * 4096, plain, k4.length * 4096) == 0 &&
          memcmp(result, orig, k4.start * 4096) == 0);
    free(result);
    unlink(merged);

    // Größe der entschlüsselten Partition passt nicht: kein merged.dd
    CHECK(merge_image(raw, wrong, &p, merged, NULL, NULL) == 0);
    CHECK(access(merged, F_OK) != 0);

    // Partition ragt über das Image hinaus
    PartitionInfo too_big = {.slot = 2, .start = TOTAL - 10, .length = LENGTH, .sector_size = SECTOR};
    CHECK(merge_image(raw, dec, &too_big, merged, NULL, NULL) == 0);
    CHECK(access(merged, F_OK) != 0);

    // Abbruch: halbe Datei wird entfernt
    volatile sig_atomic_t stop = 1;
    CHECK(merge_image(raw, dec, &p, merged, NULL, &stop) == 0);
    CHECK(access(merged, F_OK) != 0);

    // Fehlende Eingaben
    CHECK(merge_image("/gibt/es/nicht", dec, &p, merged, NULL, NULL) == 0);
    CHECK(merge_image(raw, "/gibt/es/nicht", &p, merged, NULL, NULL) == 0);
    CHECK(merge_image(raw, dec, &p, "/gibt/es/nicht/merged.dd", NULL, NULL) == 0);

    free(orig);
    free(plain);
}


/* eow */

static void put32(unsigned char *p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}

static void put64(unsigned char *p, uint64_t v)
{
    put32(p, (uint32_t)v);
    put32(p + 4, (uint32_t)(v >> 32));
}

// Prüfsumme setzen wie BitLocker: CRC32 über len Byte mit Prüfsummenfeld 0
static void seal(unsigned char *p, size_t len, size_t field)
{
    memset(p + field, 0, 4);
    put32(p + field, eow_crc32(p, len));
}

enum
{
    EOW_VOL = 0x10000,   // 64 KiB Volume
    EOW_BLOCK = 0x1000,  // 4 KiB je Bit
    EOW_DESC = 0x2000,   // Deskriptor
    EOW_LOG = 0x2800,    // Relocation-Log
    EOW_MAP = 0x3000,    // Block-Map, Records bei +0x200 und +0x400
    EOW_REGION = 0x4000, // beschriebener Bereich
    EOW_REGION_SIZE = 0xB800
};

static void eow_record(unsigned char *vol, uint32_t at, uint64_t seq, uint16_t bitmap)
{
    unsigned char *r = vol + at;
    memset(r, 0, 512);
    memcpy(r, "FVE-EOWBR\0", 10);
    r[10] = 36;
    put32(r + 12, 512);
    put32(r + 16, 12);
    put64(r + 20, seq);
    r[36] = (unsigned char)bitmap;
    r[37] = (unsigned char)(bitmap >> 8);
    seal(r, 512, 32);
}

// Volume mit EOW: Bits 0,1,4,6..10 verschlüsselt, 2,3,5,11 im Klartext.
static void build_eow_volume(unsigned char *vol)
{
    memset(vol, 0, EOW_VOL);
    memcpy(vol + 3, "-FVE-FS-", 8);
    static const unsigned char id[16] = {0x3b, 0x4d, 0xa8, 0x92, 0x80, 0xdd, 0x0e, 0x4d,
                                         0x9e, 0x4e, 0xb1, 0xe3, 0x28, 0x4e, 0xae, 0xd8};
    memcpy(vol + 0xA0, id, 16);
    put64(vol + 0xC8, EOW_DESC);

    unsigned char *d = vol + EOW_DESC;
    memcpy(d, "FVE-EOW\0", 8);
    d[8] = 56;
    d[10] = 64; // Kopf plus eine Block-Map
    put32(d + 12, 512);
    put32(d + 16, 512);
    put32(d + 20, EOW_BLOCK);
    put32(d + 24, 0x400);
    put32(d + 32, 1);
    put64(d + 40, EOW_DESC);
    put64(d + 56, EOW_MAP);
    seal(d, 64, 36);

    unsigned char *m = vol + EOW_MAP;
    memcpy(m, "FVE-EOWBM\0", 10);
    m[10] = 60;
    put32(m + 12, 0x600);
    put64(m + 20, EOW_REGION);
    put64(m + 28, EOW_REGION_SIZE);
    put64(m + 36, EOW_LOG);
    put32(m + 44, 0x200);
    put32(m + 48, 0x400);
    put32(m + 52, 512);
    seal(m, 512, 56);

    eow_record(vol, EOW_MAP + 0x200, 5, 0xFFFF);          // älter: alles verschlüsselt
    eow_record(vol, EOW_MAP + 0x400, 7, 0x7D3);           // maßgeblich
}

static void test_eow(void)
{
    CHECK(eow_crc32((const unsigned char *)"123456789", 9) == 0xCBF43926u);

    unsigned char *vol = malloc(EOW_VOL);
    char path[PATH_MAX], err[256];
    tmp_path(path, sizeof(path), "eow.img");
    EowMap map;

    build_eow_volume(vol);
    write_file(path, vol, EOW_VOL);
    int fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == 1);
    close(fd);
    CHECK(map.relocation_block_size == EOW_BLOCK && map.map_count == 1);
    CHECK(map.map_count == 1 && map.maps[0].sequence == 7 && map.maps[0].bits_set == 8);
    // Verwaltungsdaten 0x2000..0x4000 (Deskriptor, Log, Block-Map), Klartext Bits 2-3, 5, 11 (gekürzt)
    const EowRange want[] = {{0x2000, 0x2000, EOW_META},
                             {0x6000, 0x2000, EOW_PLAIN},
                             {0x9000, 0x1000, EOW_PLAIN},
                             {0xF000, 0x800, EOW_PLAIN}};
    CHECK(map.range_count == 4);
    for (size_t i = 0; i < 4 && i < map.range_count; i++)
        CHECK(map.ranges[i].offset == want[i].offset && map.ranges[i].length == want[i].length &&
              map.ranges[i].kind == want[i].kind);
    CHECK(map.plain_bytes == 0x3800 && map.meta_bytes == 0x2000);

    char report_path[PATH_MAX];
    tmp_path(report_path, sizeof(report_path), "eow.txt");
    CHECK(eow_write_report(&map, report_path) == 1);
    CHECK(eow_write_report(&map, report_path) == 0); // nie überschreiben
    size_t rlen;
    unsigned char *rep = read_file(report_path, &rlen);
    CHECK(rep && strstr((char *)rep, "0x000000006000 0x000000002000 original") != NULL);
    free(rep);

    // Zusammenführen: Bytes je Bereich aus der richtigen Quelle
    enum { OFF = 0x1000, TOTAL = OFF + EOW_VOL + 0x1000 };
    unsigned char *raw_img = malloc(TOTAL), *dec_img = malloc(EOW_VOL);
    fill_pattern(raw_img, TOTAL, 3);
    memcpy(raw_img + OFF, vol, EOW_VOL);
    fill_pattern(raw_img + OFF + EOW_REGION, EOW_REGION_SIZE, 11); // Nutzdaten im Original
    fill_pattern(dec_img, EOW_VOL, 99);
    char raw[PATH_MAX], dec[PATH_MAX], merged[PATH_MAX];
    tmp_path(raw, sizeof(raw), "eow-original.dd");
    tmp_path(dec, sizeof(dec), "eow-dislocker");
    tmp_path(merged, sizeof(merged), "eow-merged.dd");
    write_file(raw, raw_img, TOTAL);
    write_file(dec, dec_img, EOW_VOL);
    fd = open(raw, O_RDONLY);
    eow_free(&map);
    CHECK(eow_read(fd, OFF, EOW_VOL, &map, err, sizeof(err)) == 1);
    close(fd);
    PartitionInfo p = {.slot = 1, .start = OFF / 512, .length = EOW_VOL / 512, .sector_size = 512};
    unlink(merged);
    CHECK(merge_image(raw, dec, &p, merged, &map, NULL) == 1);
    size_t len;
    unsigned char *out = read_file(merged, &len);
    CHECK(out && len == TOTAL);
    if (out && len == TOTAL)
    {
        unsigned char zero[0x2000] = {0};
        const unsigned char *v = out + OFF;
        CHECK(memcmp(out, raw_img, OFF) == 0);
        CHECK(memcmp(v, dec_img, 0x2000) == 0);                       // verschlüsselt
        CHECK(memcmp(v + 0x2000, zero, 0x2000) == 0);                 // Verwaltungsdaten
        CHECK(memcmp(v + 0x4000, dec_img + 0x4000, 0x2000) == 0);     // Bits 0-1
        CHECK(memcmp(v + 0x6000, raw_img + OFF + 0x6000, 0x2000) == 0); // Bits 2-3 Klartext
        CHECK(memcmp(v + 0x8000, dec_img + 0x8000, 0x1000) == 0);     // Bit 4
        CHECK(memcmp(v + 0x9000, raw_img + OFF + 0x9000, 0x1000) == 0); // Bit 5 Klartext
        CHECK(memcmp(v + 0xA000, dec_img + 0xA000, 0x5000) == 0);     // Bits 6-10
        CHECK(memcmp(v + 0xF000, raw_img + OFF + 0xF000, 0x800) == 0);  // Bit 11, gekürzt
        CHECK(memcmp(v + 0xF800, dec_img + 0xF800, 0x800) == 0);      // hinter dem Bereich
        CHECK(memcmp(out + OFF + EOW_VOL, raw_img + OFF + EOW_VOL, 0x1000) == 0);
    }
    free(out);
    unlink(merged);
    eow_free(&map);

    // Kein EOW: anderes BitLocker-Volume und gar kein BitLocker
    unsigned char *other = malloc(EOW_VOL);
    memcpy(other, vol, EOW_VOL);
    other[0xA0] ^= 1;
    write_file(path, other, EOW_VOL);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == 0);
    close(fd);
    memset(other, 0, EOW_VOL);
    write_file(path, other, EOW_VOL);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == 0);
    close(fd);

    // Maßgeblicher Record beschädigt: älterer Record gilt (alles verschlüsselt)
    memcpy(other, vol, EOW_VOL);
    other[EOW_MAP + 0x400 + 36] ^= 0xFF;
    write_file(path, other, EOW_VOL);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == 1);
    close(fd);
    CHECK(map.plain_bytes == 0 && map.map_count == 1 && map.maps[0].sequence == 5);
    eow_free(&map);

    // Erste Deskriptor-Kopie defekt, zweite gültig
    memcpy(other, vol, EOW_VOL);
    memcpy(other + 0x1000, other + EOW_DESC, 64);
    put64(other + 0xD0, 0x1000);
    other[EOW_DESC + 20] ^= 1;
    write_file(path, other, EOW_VOL);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == 1);
    close(fd);
    CHECK(map.descriptor_offset == 0x1000 && map.plain_bytes == 0x3800);
    eow_free(&map);

    // Beschädigte Tabellen: Abbruch statt stiller Fehlentscheidung
    const struct
    {
        size_t at;
        int record_seal; // 0 Block-Map neu versiegeln, 1 Record, 2 nicht versiegeln
    } broken[] = {
        {EOW_DESC + 20, 2},        // Deskriptor ohne gültige Prüfsumme
        {EOW_MAP + 1, 2},          // Signatur der Block-Map
        {EOW_MAP + 0x400 + 16, 1}, // Bitmap mit 10 Bits deckt den Bereich nicht ab
        {EOW_MAP + 20, 0},         // Bereich außerhalb des Volumes
    };
    for (size_t i = 0; i < sizeof(broken) / sizeof(broken[0]); i++)
    {
        memcpy(other, vol, EOW_VOL);
        if (broken[i].at == EOW_MAP + 0x400 + 16)
            put32(other + broken[i].at, 10);
        else if (broken[i].at == EOW_MAP + 20)
            put64(other + EOW_MAP + 20, EOW_VOL);
        else
            other[broken[i].at] ^= 0x55;
        if (broken[i].record_seal == 0)
            seal(other + EOW_MAP, 512, 56);
        else if (broken[i].record_seal == 1)
            seal(other + EOW_MAP + 0x400, 512, 32);
        write_file(path, other, EOW_VOL);
        fd = open(path, O_RDONLY);
        CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == -1 && err[0] != '\0');
        close(fd);
    }

    // Beide Records unbrauchbar
    memcpy(other, vol, EOW_VOL);
    other[EOW_MAP + 0x200] = 'X';
    other[EOW_MAP + 0x400] = 'X';
    write_file(path, other, EOW_VOL);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == -1);
    close(fd);

    // Zu kurzes Image
    write_file(path, vol, 256);
    fd = open(path, O_RDONLY);
    CHECK(eow_read(fd, 0, EOW_VOL, &map, err, sizeof(err)) == -1);
    close(fd);

    free(other);
    free(raw_img);
    free(dec_img);
    free(vol);
}

/* dislocker_runner (nur Fehlerfälle, echte Entschlüsselung prüfen die Integrationstests) */

static void test_run_dislocker_errors(void)
{
    char img[PATH_MAX], dir[PATH_MAX];
    unsigned char zeros[4096] = {0};
    tmp_path(img, sizeof(img), "kein-bitlocker.img");
    write_file(img, zeros, sizeof(zeros));
    tmp_path(dir, sizeof(dir), "dislocker-mnt");

    CHECK(run_dislocker(img, 0, KEY_RECOVERY, "000000-000000-000000-000000-000000-000000-000000-000000", dir) == 0);
    CHECK(is_mountpoint(dir) == 0);

    tmp_path(dir, sizeof(dir), "kein-bitlocker.img/unterordner"); // Verzeichnis nicht anlegbar
    CHECK(run_dislocker(img, 0, KEY_RECOVERY, "x", dir) == 0);
}

/* build_key_args: baut die dislocker-Schlüsselargumente je nach Modus */

static void test_build_key_args(void)
{
    const char *out[2] = {NULL, NULL};
    char *heap = NULL;

    // Recovery: ein Argument "-p<key>", per malloc, muss freigegeben werden.
    int n = build_key_args(KEY_RECOVERY, "123456-654321", out, &heap);
    CHECK(n == 1);
    CHECK(heap != NULL);
    CHECK(out[0] == heap);
    CHECK(strcmp(out[0], "-p123456-654321") == 0);
    free(heap);

    // VMK: zwei Argumente "-K" und der Dateipfad, kein Heap.
    heap = NULL;
    out[0] = out[1] = NULL;
    n = build_key_args(KEY_VMK, "/pfad/blk.vmk", out, &heap);
    CHECK(n == 2);
    CHECK(heap == NULL);
    CHECK(strcmp(out[0], "-K") == 0);
    CHECK(strcmp(out[1], "/pfad/blk.vmk") == 0);

    // Ohne Schlüssel: Fehler.
    heap = NULL;
    CHECK(build_key_args(KEY_RECOVERY, NULL, out, &heap) == 0);
    CHECK(heap == NULL);
}

int main(void)
{
    report = fdopen(dup(STDOUT_FILENO), "w");
    setvbuf(report, NULL, _IOLBF, 0);
    if (!getenv("UNIT_TEST_VERBOSE"))
    {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }

    snprintf(tmp_root, sizeof(tmp_root), "/tmp/forensicunlock-unit-XXXXXX");
    if (!mkdtemp(tmp_root))
    {
        perror("mkdtemp");
        return 2;
    }

    test_run_cmd();
    test_make_dir();
    test_mountpoint();
    test_program_in_path();
    test_is_ewf_path();
    test_is_ova_path();
    test_ewf_segment_ext();
    test_check_ewf_segments();
    test_parse_mmls();
    test_bitlocker_signature();
    test_write_partition_info();
    test_image_size();
    test_copy_range();
    test_merge_image();
    test_eow();
    test_run_dislocker_errors();
    test_build_key_args();

    char *rm[] = {"rm", "-rf", tmp_root, NULL};
    run_cmd(rm);

    fprintf(report, "Unit-Tests: %d Prüfungen, %d Fehler\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
