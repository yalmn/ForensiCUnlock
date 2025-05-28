// src/image_merger.c
#define _GNU_SOURCE
#include "image_merger.h"
#include <stdio.h>
#include <stdlib.h>     
#include <stdint.h>
#include <string.h>
#include <unistd.h>     

int merge_and_cleanup(const char *raw_image, const char *dislocker_file, const PartitionInfo *bdp_info, const char *output_dir)
{
    char part1_path[512], part2_path[512], part3_path[512], merged_path[512];
    snprintf(part1_path, sizeof(part1_path), "%s/part1.img", output_dir);
    snprintf(part2_path, sizeof(part2_path), "%s/part2.img", output_dir);
    snprintf(part3_path, sizeof(part3_path), "%s/part3.img", output_dir);
    snprintf(merged_path, sizeof(merged_path), "%s/merged.dd", output_dir);

    // 1. Extrahiere part1.img
    char cmd1[1024];
    snprintf(cmd1, sizeof(cmd1),
             "dd if='%s' of='%s' bs=512 count=%llu status=none",
             raw_image, part1_path,
             (unsigned long long)bdp_info->start);

    // 2. Kopiere dislocker-file zu part2.img
    char cmd2[1024];
    snprintf(cmd2, sizeof(cmd2),
             "cp '%s' '%s'",
             dislocker_file, part2_path);

    // 3. Ermittle Gesamtlänge des Images in Sektoren
    char cmd_size[1024];
    snprintf(cmd_size, sizeof(cmd_size), "blockdev --getsz '%s'", raw_image);
    FILE *fp = popen(cmd_size, "r");
    if (!fp)
    {
        perror("[!] Fehler beim Auslesen der Imagegröße");
        return 0;
    }

    unsigned long long total_sectors = 0;
    if (fscanf(fp, "%llu", &total_sectors) != 1)
    {
        perror("[!] Fehler beim Parsen der Imagegröße");
        pclose(fp);
        return 0;
    }
    pclose(fp);

    // 4. Extrahiere part3.img
    uint64_t after_bdp = bdp_info->start + bdp_info->length;
    uint64_t tail_len = total_sectors > after_bdp ? total_sectors - after_bdp : 0;

    char cmd3[1024];
    snprintf(cmd3, sizeof(cmd3),
             "dd if='%s' of='%s' bs=512 skip=%llu count=%llu status=none",
             raw_image, part3_path,
             (unsigned long long)after_bdp,
             (unsigned long long)tail_len);

    // 5. Führe dd- und cp-Befehle aus
    printf("[*] Erzeuge Partitionsteil 1...\n");
    if (system(cmd1) != 0) return 0;

    printf("[*] Kopiere entschlüsselte Partition...\n");
    if (system(cmd2) != 0) return 0;

    printf("[*] Erzeuge Partitionsteil 3...\n");
    if (tail_len > 0 && system(cmd3) != 0) return 0;

    // 6. Merge zu merged.dd
    char cmd_merge[1024];
    snprintf(cmd_merge, sizeof(cmd_merge),
             "cat '%s' '%s' '%s' > '%s'",
             part1_path, part2_path, part3_path, merged_path);

    printf("[*] Merging nach: %s\n", merged_path);
    if (system(cmd_merge) != 0) return 0;

    // 7. Cleanup: Nur merged.dd und ursprüngliches Image bleiben erhalten
    char cmd_cleanup[1024];
    snprintf(cmd_cleanup, sizeof(cmd_cleanup),
             "rm -f '%s' '%s' '%s' '%s' && rm -rf '%s/xmount' '%s/bitlocker'",
             part1_path, part2_path, part3_path, dislocker_file, output_dir, output_dir);

    printf("[*] Bereinige temporäre Dateien...\n");
    system(cmd_cleanup);

    return 1;
}
