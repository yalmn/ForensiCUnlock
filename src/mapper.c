#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mapper.h"

int create_mapping_file(const char *loop_orig, const char *loop_decrypted,
                        uint64_t bdp_start, uint64_t bdp_length,
                        const char *output_path)
{
    char map_path[512];
    snprintf(map_path, sizeof(map_path), "%s/dmsetup.txt", output_path);
    FILE *fp = fopen(map_path, "w");
    if (!fp) {
        perror("Fehler beim Erstellen der Mapping-Datei");
        return 0;
    }

    fprintf(fp, "0 %llu linear %s 0\n", (unsigned long long)bdp_start, loop_orig);
    fprintf(fp, "%llu %llu linear %s 0\n", (unsigned long long)bdp_start, (unsigned long long)bdp_length, loop_decrypted);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "blockdev --getsz %s", loop_orig);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        perror("Fehler beim Ausführen von blockdev");
        fclose(fp);
        return 0;
    }

    unsigned long total_sectors;
    fscanf(pipe, "%lu", &total_sectors);
    pclose(pipe);

    uint64_t rest_start = bdp_start + bdp_length;
    uint64_t rest_length = total_sectors - rest_start;

    fprintf(fp, "%llu %llu linear %s %llu\n",
            (unsigned long long)rest_start,
            (unsigned long long)rest_length,
            loop_orig,
            (unsigned long long)rest_start);

    fclose(fp);
    return 1;
}

int setup_dm_device(const char *output_path)
{
    char map_path[512];
    snprintf(map_path, sizeof(map_path), "%s/dmsetup.txt", output_path);
    char command[600];
    snprintf(command, sizeof(command), "dmsetup create merged < '%s'", map_path);
    printf("Erzeuge Device /dev/mapper/merged via: %s\n", command);
    return system(command) == 0;
}
