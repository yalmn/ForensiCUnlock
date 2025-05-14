#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "partition_parser.h"

int find_bdp_partition(const char *image_path, PartitionInfo *info)
{
    char command[512];
    snprintf(command, sizeof(command), "mmls %s", image_path);

    FILE *fp = popen(command, "r");
    if (!fp) {
        perror("[!] Fehler beim Ausführen von mmls");
        return 0;
    }

    char line[512];
    int slot = -1;
    uint64_t start = 0, length = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "NTFS") || strstr(line, "Basic data")) {
            if (sscanf(line, "%d:%*[^0-9]%" SCNu64 "%*s%*s%" SCNu64, &slot, &start, &length) == 3) {
                info->slot = slot;
                info->start = start;
                info->length = length;
                pclose(fp);
                return 1;
            }
        }
    }

    pclose(fp);
    fprintf(stderr, "[!] Keine geeignete NTFS/BDP Partition gefunden.\n");
    return 0;
}
