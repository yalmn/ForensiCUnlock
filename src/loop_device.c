#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "loop_device.h"

int setup_loop_device(const char *file, char *loopdev, size_t size)
{
    char command[256];
    snprintf(command, sizeof(command), "losetup --find --show %s", file);
    FILE *fp = popen(command, "r");
    if (!fp)
        return 0;

    if (fgets(loopdev, size, fp) == NULL) {
        pclose(fp);
        return 0;
    }

    loopdev[strcspn(loopdev, "\n")] = 0;
    pclose(fp);
    return 1;
}
