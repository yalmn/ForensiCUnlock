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
    snprintf(cmd, sizeof(cmd), "mmls -i raw '%s'", image_path);

    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        perror("[!] Fehler beim Ausführen von mmls");
        return 0;
    }

    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "Basic data partition"))
        {
            // Slotnummer ignorieren – wir holen nur Start/Ende
            char *start_str = strtok(line, " \t"); // Slot (z.B. "006:")
            start_str = strtok(NULL, " \t"); // ID (z.B. "002")
            start_str = strtok(NULL, " \t"); // Start

            char *end_str = strtok(NULL, " \t");   // End
            if (start_str && end_str)
            {
                uint64_t start = strtoull(start_str, NULL, 10);
                uint64_t end = strtoull(end_str, NULL, 10);

                info->slot = -1; // nicht bekannt
                info->start = start;
                info->length = end - start + 1;

                // Save BDP info
                FILE *out = fopen("./bdp.info", "w");
                if (out)
                {
                    fprintf(out, "start=%llu\nend=%llu\nlength=%llu\n",
                            (unsigned long long)start,
                            (unsigned long long)end,
                            (unsigned long long)(end - start + 1));
                    fclose(out);
                    printf("[*] Partitioninfo gespeichert in: ./bdp.info\n");
                }

                printf("[*] BDP erkannt über mmls: Start=%llu Ende=%llu Länge=%llu\n",
                       (unsigned long long)start,
                       (unsigned long long)end,
                       (unsigned long long)(end - start + 1));
                found = 1;
                break;
            }
        }
    }

    pclose(fp);

    if (!found)
    {
        fprintf(stderr, "[!] Keine 'Basic data partition' über mmls gefunden.\n");
        return 0;
    }

    return 1;
}
