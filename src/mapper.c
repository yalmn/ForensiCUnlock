// src/mapper.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mapper.h"

#define MAPFILE_PATH "/mnt/output/dmsetup.txt"

int create_mapping_file(const char *loop_orig, const char *loop_decrypted, uint64_t bdp_start, uint64_t bdp_length)
{
    FILE *fp = fopen(MAPFILE_PATH, "w");
    if (!fp)
    {
        perror("Fehler beim Erstellen der Mapping-Datei");
        return 0;
    }

    // Abschnitt vor BDP
    fprintf(fp, "0 %llu linear %s 0\n", bdp_start, loop_orig);

    // Entschlüsselter BDP-Bereich
    fprintf(fp, "%llu %llu linear %s 0\n", bdp_start, bdp_length, loop_decrypted);

    // Bereich nach BDP
    // Hole Gesamtgröße des Images über blockdev
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "blockdev --getsz %s", loop_orig);
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
    {
        perror("Fehler bei blockdev");
        fclose(fp);
        return 0;
    }

    unsigned long long total_sectors = 0;
    fscanf(pipe, "%llu", &total_sectors);
    pclose(pipe);

    uint64_t rest_start = bdp_start + bdp_length;
    uint64_t rest_length = total_sectors - rest_start;

    fprintf(fp, "%llu %llu linear %s %llu\n", rest_start, rest_length, loop_orig, rest_start);

    fclose(fp);
    printf("Mapping-Datei erfolgreich erstellt: %s\n", MAPFILE_PATH);
    return 1;
}

int setup_dm_device()
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dmsetup create merged < %s", MAPFILE_PATH);
    int ret = system(cmd);
    if (ret != 0)
    {
        fprintf(stderr, "Fehler bei dmsetup create. Code: %d\n", ret);
        return 0;
    }
    printf("Gerät /dev/mapper/merged wurde erfolgreich erstellt.\n");
    return 1;
}