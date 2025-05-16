// partition_parser.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "partition_parser.h"

int find_bdp_partition(const char *image_path, PartitionInfo *info)
{
    char cmd[512];
    // Machine-Mode, Sektor-Einheiten
    snprintf(cmd, sizeof(cmd),
             "parted -m -s %s unit s print", image_path);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        perror("[!] Fehler beim Ausführen von parted");
        return 0;
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp))
    {
        // Kopfzeilen (Pfad) überspringen
        if (line[0] == '/' || strchr(line, ':') == NULL)
            continue;

        int slot;
        uint64_t start_s, end_s;
        char fs[32];

        // slot:start<s>:end<s>:fs:...
        if (sscanf(line, "%d:%" SCNu64 "s:%" SCNu64 "s:%31[^:]:",
                   &slot, &start_s, &end_s, fs) == 4)
        {
            // NTFS oder Basic data?
            if (strcasecmp(fs, "ntfs") == 0 || strcasecmp(fs, "basic data") == 0)
            {
                info->slot = slot;
                info->start = start_s;
                info->length = end_s - start_s + 1;
                found = 1;
                break;
            }
        }
    }

    pclose(fp);

    if (!found)
    {
        fprintf(stderr, "[!] Keine NTFS/BDP-Partition gefunden.\n");
        return 0;
    }

    return 1;
}
