#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "mount_selector.h"
#include "image_converter.h"
#include "partition_parser.h"
#include "dislocker_runner.h"
#include "loop_device.h"
#include "mapper.h"

int main(int argc, char *argv[])
{
    if (geteuid() != 0 || getenv("SUDO_USER") == NULL)
    {
        fprintf(stderr, "[!] Fehler: Bitte führe das Programm mit 'sudo' aus.\n");
        return 1;
    }

    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <device|image> <bitlocker_key> <output_folder>\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    const char *bitlocker_key = argv[2];
    const char *output_folder = argv[3];

    char selected_image_dir[512];
    char xmount_path[512];
    char bitlocker_path[512];
    char mapping_file[512];
    char raw_image_path[1024];
    char dislocker_file[1024];

    snprintf(xmount_path, sizeof(xmount_path), "%s/xmount", output_folder);
    snprintf(bitlocker_path, sizeof(bitlocker_path), "%s/bitlocker", output_folder);
    snprintf(mapping_file, sizeof(mapping_file), "%s/dmsetup.txt", output_folder);
    snprintf(raw_image_path, sizeof(raw_image_path), "%s/image.dd", xmount_path);
    snprintf(dislocker_file, sizeof(dislocker_file), "%s/dislocker-file", bitlocker_path);

    printf("[*] Arbeitsordner: %s\n", output_folder);
    mkdir(output_folder, 0755);

    printf("[*] Mountvorgang für Gerät: %s\n", device);
    if (!auto_mount_and_find_ewf(device, selected_image_dir, sizeof(selected_image_dir)))
    {
        fprintf(stderr, "[!] Fehler beim automatischen Mounten des Beweismittels.\n");
        return 1;
    }
    printf("[*] Gefundenes EWF-Verzeichnis: %s\n", selected_image_dir);

    int has_ewf = 0;
    DIR *dir = opendir(selected_image_dir);
    struct dirent *entry;
    if (dir)
    {
        while ((entry = readdir(dir)) != NULL)
        {
            if (strstr(entry->d_name, ".E01") || strstr(entry->d_name, ".ewf"))
            {
                has_ewf = 1;
                break;
            }
        }
        closedir(dir);
    }

    if (has_ewf)
    {
        printf("[*] EWF-Datei erkannt. Starte Konvertierung...\n");
        if (!convert_ewf_to_raw(selected_image_dir, xmount_path))
        {
            fprintf(stderr, "[!] Fehler: Konvertierung nach RAW fehlgeschlagen.\n");
            return 1;
        }
    }
    else
    {
        printf("[*] Keine EWF-Datei erkannt. Verwende vorhandenes RAW: %s\n", raw_image_path);
    }

    printf("[*] Analysiere Partitionstabelle...\n");
    PartitionInfo bdp_info;
    if (!find_bdp_partition(raw_image_path, &bdp_info))
    {
        fprintf(stderr, "[!] Fehler beim Parsen der BDP-Partition.\n");
        return 1;
    }

    printf("[*] BDP erkannt: Slot %d, Start: %llu, Länge: %llu\n",
           bdp_info.slot,
           (unsigned long long)bdp_info.start,
           (unsigned long long)bdp_info.length);

    printf("[*] Zu prüfender Bereich → Offset: %llu Sektoren, Länge: %llu Sektoren\n",
           (unsigned long long)bdp_info.start,
           (unsigned long long)bdp_info.length);

    {
        char mmls_cmd[1024];
        snprintf(mmls_cmd, sizeof(mmls_cmd),
                 "mmls -i raw %s", raw_image_path);
        printf("[*] mmls-Partitionstabelle:\n");
        system(mmls_cmd);
    }

    printf("[*] Kontrolliere mmls-Tabelle und bestätige mit ENTER...\n");
    system("read -p '>>> Weiter mit ENTER...'");

    printf("[*] Entschlüsselung der BitLocker-Partition gestartet...\n");
    if (!run_dislocker(raw_image_path, bdp_info.start, bitlocker_key, bitlocker_path))
    {
        fprintf(stderr, "[!] Fehler bei der BitLocker-Entschlüsselung.\n");
        return 1;
    }

    char loop_decrypted[64];
    char loop_original[64];

    if (!setup_loop_device(dislocker_file, loop_decrypted, sizeof(loop_decrypted)))
    {
        fprintf(stderr, "[!] Fehler beim Einrichten des Loop-Devices (dislocker).\n");
        return 1;
    }

    if (!setup_loop_device(raw_image_path, loop_original, sizeof(loop_original)))
    {
        fprintf(stderr, "[!] Fehler beim Einrichten des Loop-Devices (RAW).\n");
        return 1;
    }

    printf("[*] Loop-Device (entschlüsselt): %s\n", loop_decrypted);
    printf("[*] Loop-Device (original):     %s\n", loop_original);

    if (!create_mapping_file(loop_original, loop_decrypted, bdp_info.start, bdp_info.length, output_folder))
    {
        fprintf(stderr, "[!] Fehler beim Schreiben der Mapping-Datei.\n");
        return 1;
    }

    if (!setup_dm_device(output_folder))
    {
        fprintf(stderr, "[!] Fehler bei der Erstellung des gemergten Devices.\n");
        return 1;
    }

    printf("\n[✔] ForensiCUnlock abgeschlossen.\n");
    printf("Arbeitsverzeichnis: %s\n", output_folder);
    printf("RAW-Image:          %s\n", raw_image_path);
    printf("Entschlüsseltes:    %s\n", dislocker_file);
    printf("Mapping-Datei:      %s\n", mapping_file);
    printf("Gemergtes Gerät:    /dev/mapper/merged\n");

    return 0;
}
