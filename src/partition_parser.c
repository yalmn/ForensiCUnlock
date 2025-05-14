// src/partition_parser.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "partition_parser.h"

int find_bdp_partition(const char *image_path, PartitionInfo *bdp_info)
{
    char command[512];
    snprintf(command, sizeof(command), "mmls %s", image_path);

    FILE *fp = popen(command, "r");
    if (!fp)
    {
        perror("Fehler beim Starten von mmls");
        return 0;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "Basic data partition"))
        {
            int slot;
            unsigned long long start, length;
            if (sscanf(line, "%d:%*[^0-9]%llu%*s%*s%llu", &slot, &start, &length) == 3)
            {
                bdp_info->slot = slot;
                bdp_info->start = start;
                bdp_info->length = length;
                pclose(fp);
                return 1;
            }
        }
    }

    pclose(fp);
    fprintf(stderr, "Basic data partition nicht gefunden.\n");
    return 0;
}