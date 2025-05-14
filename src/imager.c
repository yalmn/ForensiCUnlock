// src/imager.c
#include <stdio.h>
#include <stdlib.h>
#include "imager.h"

#define OUTPUT_PATH "/mnt/output/merged_image.dd"
#define LOG_PATH "/mnt/output/merged_image.dd.log"

int create_final_image()
{
    char command[1024];
    snprintf(command, sizeof(command), "ddrescue -b 512 /dev/mapper/merged %s %s", OUTPUT_PATH, LOG_PATH);

    printf("Starte ddrescue zur Erstellung des finalen Images...\n");
    int ret = system(command);
    if (ret != 0)
    {
        fprintf(stderr, "Fehler bei ddrescue. Rückgabewert: %d\n", ret);
        return 0;
    }

    printf("Finales Image erfolgreich erstellt: %s\n", OUTPUT_PATH);
    return 1;
}
