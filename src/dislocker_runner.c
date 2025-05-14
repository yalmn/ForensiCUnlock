// src/dislocker_runner.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dislocker_runner.h"

#define BITLOCKER_OUT "/mnt/bitlocker"

int run_dislocker(const char *image_path, uint64_t start_sector, const char *key)
{
    char command[1024];
    long long offset = start_sector * 512LL;

    printf("Starte dislocker zur Entschlüsselung ab Offset %lld...\n", offset);

    // Erstelle Zielverzeichnis
    mkdir(BITLOCKER_OUT, 0755);

    // dislocker-Aufruf vorbereiten
    snprintf(command, sizeof(command),
             "dislocker -V %s -O %lld -p%s -r -- %s",
             image_path, offset, key, BITLOCKER_OUT);

    // Ausführen
    int ret = system(command);
    if (ret != 0)
    {
        fprintf(stderr, "dislocker fehlgeschlagen. Rückgabewert: %d\n", ret);
        return 0;
    }

    // Prüfen ob dislocker-file existiert
    char dis_path[512];
    snprintf(dis_path, sizeof(dis_path), "%s/dislocker-file", BITLOCKER_OUT);

    if (access(dis_path, F_OK) != 0)
    {
        fprintf(stderr, "Fehler: dislocker-file nicht gefunden unter %s\n", dis_path);
        return 0;
    }

    printf("Entschlüsselung erfolgreich: %s\n", dis_path);
    return 1;
}
