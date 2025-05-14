// src/loop_device.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "loop_device.h"

int setup_loop_device(const char *file_path, char *loop_path, size_t max_len)
{
    char command[512];
    FILE *fp;

    snprintf(command, sizeof(command), "losetup --find --show %s", file_path);
    fp = popen(command, "r");
    if (!fp)
    {
        perror("Fehler beim Aufruf von losetup");
        return 0;
    }

    if (fgets(loop_path, max_len, fp) == NULL)
    {
        fprintf(stderr, "Fehler: Kein Loop-Gerät zurückgegeben.\n");
        pclose(fp);
        return 0;
    }

    // Zeilenumbruch entfernen
    loop_path[strcspn(loop_path, "\n")] = 0;
    pclose(fp);
    return 1;
}