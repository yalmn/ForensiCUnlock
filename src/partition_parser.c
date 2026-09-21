// src/partition_parser.c
#include "partition_parser.h"
#include "exec_utils.h"
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// BitLocker-GUID {4967d63b-2e29-4ad8-8399-f6a339e3d001} wie sie auf dem Datenträger liegt
static const unsigned char BITLOCKER_GUID[16] = {
    0x3b, 0xd6, 0x67, 0x49, 0x29, 0x2e, 0xd8, 0x4a,
    0x83, 0x99, 0xf6, 0xa3, 0x39, 0xe3, 0xd0, 0x01};

int parse_mmls_units(const char *line, uint32_t *sector_size)
{
    unsigned long size;
    if (sscanf(line, " Units are in %lu-byte sectors", &size) != 1 || size == 0 || size > UINT32_MAX)
        return 0;
    *sector_size = (uint32_t)size;
    return 1;
}

int parse_mmls_entry(const char *line, PartitionInfo *info)
{
    // Aufbau: "005:  001   0000002048   0000206847   0000204800   Beschreibung"
    int slot, desc_pos = 0;
    char addr[32];
    uint64_t start, end, length;

    if (sscanf(line, " %d: %31s %" SCNu64 " %" SCNu64 " %" SCNu64 " %n",
               &slot, addr, &start, &end, &length, &desc_pos) != 5)
        return 0;

    // "Meta" und "-------" sind keine Partitionen
    if (!isdigit((unsigned char)addr[0]))
        return 0;
    if (length == 0 || end < start || end - start + 1 != length)
        return 0;

    info->slot = slot;
    info->start = start;
    info->length = length;

    const char *desc = desc_pos > 0 ? line + desc_pos : "";
    snprintf(info->description, sizeof(info->description), "%s", desc);
    info->description[strcspn(info->description, "\r\n")] = '\0';
    return 1;
}

int has_bitlocker_signature(const char *image_path, uint64_t offset)
{
    int fd = open(image_path, O_RDONLY);
    if (fd < 0)
        return 0;

    unsigned char buf[512];
    ssize_t n = pread(fd, buf, sizeof(buf), (off_t)offset);
    close(fd);
    if (n != (ssize_t)sizeof(buf))
        return 0;

    if (memcmp(buf + 3, "-FVE-FS-", 8) == 0)
        return 1;
    return memcmp(buf + 0x1a8, BITLOCKER_GUID, sizeof(BITLOCKER_GUID)) == 0;
}

int find_bitlocker_partitions(const char *image_path, PartitionInfo *list, int max)
{
    char *argv[] = {"mmls", "-i", "raw", (char *)image_path, NULL};
    pid_t pid;
    FILE *fp = run_cmd_read(argv, &pid);
    if (!fp)
        return -1;

    PartitionInfo all[MAX_PARTITIONS];
    int total = 0;
    uint32_t sector_size = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp))
    {
        if (parse_mmls_units(line, &sector_size))
            continue;
        if (total < MAX_PARTITIONS && parse_mmls_entry(line, &all[total]))
            total++;
    }

    if (!close_cmd_read(fp, pid))
    {
        fprintf(stderr, "[!] mmls konnte keine Partitionstabelle lesen.\n");
        return -1;
    }
    if (sector_size == 0)
    {
        fprintf(stderr, "[!] Sektorgröße fehlt in der mmls-Ausgabe.\n");
        return -1;
    }

    int found = 0;
    for (int i = 0; i < total && found < max; i++)
    {
        all[i].sector_size = sector_size;
        if (has_bitlocker_signature(image_path, all[i].start * sector_size))
            list[found++] = all[i];
    }
    return found;
}

int write_partition_info(const char *path, const PartitionInfo *info)
{
    FILE *out = fopen(path, "w");
    if (!out)
        return 0;

    fprintf(out, "slot=%d\n", info->slot);
    fprintf(out, "start=%" PRIu64 "\n", info->start);
    fprintf(out, "end=%" PRIu64 "\n", info->start + info->length - 1);
    fprintf(out, "length=%" PRIu64 "\n", info->length);
    fprintf(out, "sector_size=%" PRIu32 "\n", info->sector_size);
    fprintf(out, "offset_bytes=%" PRIu64 "\n", info->start * info->sector_size);
    return fclose(out) == 0;
}
