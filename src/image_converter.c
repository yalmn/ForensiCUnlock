// src/image_converter.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "image_converter.h"

#define XMOUNT_DIR "/mnt/xmount"

int convert_ewf_to_raw(const char *ewf_dir)
{
    char command[1024];
    struct stat st;

    printf("Starte xmount zur Umwandlung von EWF nach RAW...\n");

    // Zielverzeichnis erstellen
    mkdir(XMOUNT_DIR, 0755);

    // Suche nach image.E01 im angegebenen Verzeichnis
    char image_path[512];
    snprintf(image_path, sizeof(image_path), "%s/image.E01", ewf_dir);

    if (access(image_path, F_OK) != 0)
    {
        fprintf(stderr, "Fehler: Datei %s wurde nicht gefunden.\n", image_path);
        return 0;
    }

    // xmount-Kommando vorbereiten
    snprintf(command, sizeof(command), "xmount --in ewf %s --out raw %s", image_path, XMOUNT_DIR);

    // xmount ausführen
    int ret = system(command);
    if (ret != 0)
    {
        fprintf(stderr, "xmount-Kommando fehlgeschlagen. Code: %d\n", ret);
        return 0;
    }

    // Prüfen, ob image.dd erzeugt wurde
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/image.dd", XMOUNT_DIR);
    if (stat(output_path, &st) != 0)
    {
        fprintf(stderr, "Fehler: RAW-Image %s nicht gefunden.\n", output_path);
        return 0;
    }

    printf("RAW-Image erfolgreich erstellt: %s\n", output_path);
    return 1;
}