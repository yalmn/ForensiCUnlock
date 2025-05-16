// src/image_converter.c
#include "image_converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int convert_ewf_to_raw(const char *ewf_dir, const char *xmount_dir)
{
    char command[1024];
    // Erstelle Zielverzeichnis und starte xmount für EWF → RAW
    snprintf(command, sizeof(command),
             "mkdir -p '%s' && xmount --in ewf '%s'/image.E01 --out raw '%s'",
             xmount_dir, ewf_dir, xmount_dir);

    printf("[*] Starte EWF-Konvertierung: %s\n", command);
    int result = system(command);

    // Prüfe, ob das RAW-Image erstellt wurde
    char raw_path[1024];
    snprintf(raw_path, sizeof(raw_path), "%s/image.dd", xmount_dir);
    if (access(raw_path, F_OK) != 0)
    {
        fprintf(stderr, "[!] RAW-Image wurde nicht gefunden unter %s\n", raw_path);
        return 0;
    }

    return (result == 0);
}