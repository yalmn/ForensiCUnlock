#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/wait.h>
#include "mapper.h"

int create_mapping_file(const char *loop_orig, const char *loop_decrypted, uint64_t bdp_start, uint64_t bdp_length)
{
    FILE *fp = fopen("/mnt/output/dmsetup.txt", "w");
    if (!fp) return 0;

    fprintf(fp, "0 %lu linear %s 0\n", (unsigned long)bdp_start, loop_orig);
    fprintf(fp, "%lu %lu linear %s 0\n", (unsigned long)bdp_start, (unsigned long)bdp_length, loop_decrypted);

    char cmd[] = "blockdev --getsz /mnt/xmount/image.dd";
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return 0;

    unsigned long total_sectors = 0;
    fscanf(pipe, "%lu", &total_sectors);
    pclose(pipe);

    unsigned long rest_start = bdp_start + bdp_length;
    unsigned long rest_length = total_sectors - rest_start;
    fprintf(fp, "%lu %lu linear %s %lu\n", rest_start, rest_length, loop_orig, rest_start);

    fclose(fp);
    return 1;
}

int setup_dm_device()
{
    int status = system("dmsetup create merged /mnt/output/dmsetup.txt");
    if (status == -1) {
        perror("Fehler bei dmsetup");
        return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
