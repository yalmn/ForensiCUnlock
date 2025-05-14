// src/mount_selector.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>
#include "mount_selector.h"

#define MOUNT_POINT "/mnt/output"

int auto_mount_and_find_ewf(const char *device, char *selected_dir, size_t max_len)
{
    DIR *base;
    struct dirent *entry;

    printf("[*] Erstelle Mountpunkt %s falls nicht vorhanden...\n", MOUNT_POINT);
    mkdir(MOUNT_POINT, 0755);

    printf("[*] Versuche %s nach %s zu mounten...\n", device, MOUNT_POINT);
    if (mount(device, MOUNT_POINT, "auto", MS_RDONLY, "") != 0)
    {
        perror("[!] Fehler beim Mounten");
        return 0;
    }

    printf("[*] Durchsuche Unterverzeichnisse nach EWF-Dateien...\n");
    base = opendir(MOUNT_POINT);
    if (!base)
    {
        perror("[!] Fehler beim Öffnen des Mountpunktes");
        return 0;
    }

    while ((entry = readdir(base)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, entry->d_name);

            DIR *sub = opendir(path);
            if (!sub)
                continue;

            struct dirent *subentry;
            while ((subentry = readdir(sub)) != NULL)
            {
                if (strstr(subentry->d_name, "image.E") != NULL)
                {
                    strncpy(selected_dir, path, max_len - 1);
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
