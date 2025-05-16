// src/mount_selector.c
#define _GNU_SOURCE
#include "mount_selector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>

#define MOUNT_POINT "/mnt/output"

// Helfer: Dateiendung aus Pfad extrahieren
static const char *get_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return (dot && dot != path) ? dot : "";
}

int auto_mount_and_find_ewf(const char *device, char *selected_dir, size_t max_len)
{
    DIR *base;
    struct dirent *entry;
    struct stat st;
    char cmd[1024];

    // 1. Sicherstellen, dass Mountpunkt existiert
    printf("[*] Erstelle Mountpunkt %s falls nicht vorhanden...\n", MOUNT_POINT);
    if (mkdir(MOUNT_POINT, 0755) != 0 && errno != EEXIST)
    {
        perror("[!] Fehler beim Anlegen des Mountpunkts");
        return 0;
    }

    // 2. Erkennen des Eingabe-Typs: EWF-Container, Roh-Image oder Block-Device
    if (stat(device, &st) == 0 && S_ISREG(st.st_mode))
    {
        const char *ext = get_extension(device);
        // EWF-Container
        if (strcasecmp(ext, ".E01") == 0 || strcasecmp(ext, ".ewf") == 0)
        {
            printf("[*] Erkannt: EWF-Container. ewfmount '%s' nach %s...\n", device, MOUNT_POINT);
            snprintf(cmd, sizeof(cmd), "ewfmount '%s' %s", device, MOUNT_POINT);
            if (system(cmd) != 0)
            {
                perror("[!] Fehler beim Mounten der EWF-Datei");
                return 0;
            }
        }
        // Roh-Image
        else
        {
            printf("[*] Erkannt: Roh-Image. Loop-Mount read-only...\n");
            snprintf(cmd, sizeof(cmd), "mount -o loop,ro '%s' %s", device, MOUNT_POINT);
            if (system(cmd) != 0)
            {
                perror("[!] Fehler beim Loop-Mount des Image-Files");
                return 0;
            }
        }
    }
    // Block-Device
    else
    {
        printf("[*] Erkannt: Block-Device. Mount read-only...\n");
        if (mount(device, MOUNT_POINT, "auto", MS_RDONLY, NULL) != 0)
        {
            perror("[!] Fehler beim Mounten des Block-Devices");
            return 0;
        }
    }

    // 3. Suche nach EWF-Splits in Unterverzeichnissen
    printf("[*] Durchsuche Unterverzeichnisse von %s nach EWF-Splits...\n", MOUNT_POINT);
    base = opendir(MOUNT_POINT);
    if (!base)
    {
        perror("[!] Fehler beim Öffnen des Mountpunktes");
        return 0;
    }

    while ((entry = readdir(base)) != NULL)
    {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", MOUNT_POINT, entry->d_name);

        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            DIR *sub = opendir(fullpath);
            if (!sub)
                continue;

            struct dirent *subentry;
            while ((subentry = readdir(sub)) != NULL)
            {
                if (strstr(subentry->d_name, "image.E") != NULL)
                {
                    // Auswahl des Verzeichnisses mit EWF-Files
                    strncpy(selected_dir, fullpath, max_len - 1);
                    selected_dir[max_len - 1] = '\0';
                    closedir(sub);
                    closedir(base);
                    return 1;
                }
            }
            closedir(sub);
        }
    }
    closedir(base);

    fprintf(stderr, "[!] Kein Verzeichnis mit image.E* gefunden.\n");
    return 0;
}
