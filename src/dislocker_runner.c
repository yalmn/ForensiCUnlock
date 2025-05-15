#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dislocker_runner.h"

int run_dislocker(const char *image_path, uint64_t start_sector, const char *key, const char *output_path) {
    char command[1024];
    uint64_t offset = start_sector * 512;

    snprintf(command, sizeof(command),
             "mkdir -p '%s' && dislocker -V '%s' -O %lu -p'%s' -r -- '%s'",
             output_path, image_path, offset, key, output_path);

    printf("Starte dislocker:\n%s\n", command);
    return system(command) == 0;
}
