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
    snprintf(cmd, sizeof(cmd), "parted -m -s '%s' unit s print", image_path);

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
        if (line[0] == '/' || strchr(line, ':') == NULL)
            continue;

        // Tokenisiere Zeile per ':'
        char *tokens[8] = {0};
        int i = 0;
        char *ptr = strtok(line, ":");
        while (ptr && i < 8)
        {
            tokens[i++] = ptr;
            ptr = strtok(NULL, ":");
        }

        if (i < 6)
            continue;

        // Extrahiere relevante Felder
        int slot = atoi(tokens[0]);
        uint64_t start_s = strtoull(tokens[1], NULL, 10);
        uint64_t end_s = strtoull(tokens[2], NULL, 10);
        const char *desc = tokens[5];

        // Prüfe Beschreibung auf "basic data" oder "ntfs"
        if (strcasestr(desc, "basic data") || strcasestr(desc, "ntfs"))
        {
            info->slot = slot;
            info->start = start_s;
            info->length = end_s - start_s + 1;
            found = 1;
            break;
        }
    }

    pclose(fp);

    if (!found)
    {
        fprintf(stderr, "[!] Keine geeignete BDP-Partition gefunden (basic data/ntfs).\n");
        return 0;
    }

    return 1;
}
