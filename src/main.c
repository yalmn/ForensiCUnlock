#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "forensic.h"
#include "mount_selector.h"
#include "image_converter.h"
#include "partition_parser.h"
#include "dislocker_runner.h"
#include "loop_device.h"
#include "mapper.h"
#include "imager.h"

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        fprintf(stderr, "Fehler: Bitte mit 'sudo' oder als Root ausführen.\n");
        return 1;
    }

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <device> <bitlocker_key> <output_dir>\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    const char *bitlocker_key = argv[2];
    const char *run_dir = argv[3];

    char xmount_path[512];
    char bitlocker_path[512];
    snprintf(xmount_path, sizeof(xmount_path), "%s/xmount", run_dir);
    snprintf(bitlocker_path, sizeof(bitlocker_path), "%s/bitlocker", run_dir);

    mkdir(run_dir, 0755);
    mkdir(xmount_path, 0755);
    mkdir(bitlocker_path, 0755);

    printf("Arbeitsverzeichnis gesetzt: %s\n", run_dir);
    printf("XMount-Verzeichnis:         %s\n", xmount_path);
    printf("BitLocker-Ausgabe:          %s\n\n", bitlocker_path);

    char selected_image_dir[512];
    if (!auto_mount_and_find_ewf(device, selected_image_dir, sizeof(selected_image_dir))) {
        fprintf(stderr, "EWF-Verzeichnis konnte nicht gefunden werden.\n");
        return 1;
    }

    char raw_image_path[512];
    snprintf(raw_image_path, sizeof(raw_image_path), "%s/image.dd", xmount_path);

    DIR *dir = opendir(selected_image_dir);
    struct dirent *entry;
    int has_ewf = 0;
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".E01") || strstr(entry->d_name, ".ewf") || strstr(entry->d_name, ".EWF")) {
                has_ewf = 1;
                break;
            }
        }
        closedir(dir);
    }

    if (has_ewf) {
        printf("EWF-Datei erkannt. Konvertierung nach RAW wird durchgeführt...\n");
        if (!convert_ewf_to_raw(selected_image_dir, xmount_path)) {
            fprintf(stderr, "Fehler bei Konvertierung von EWF zu RAW.\n");
            return 1;
        }
    } else {
        printf("Kein EWF gefunden. Verwende vorhandenes RAW-Image: %s\n", raw_image_path);
    }

    PartitionInfo bdp_info;
    if (!find_bdp_partition(raw_image_path, &bdp_info)) {
        fprintf(stderr, "Basic Data Partition konnte nicht ermittelt werden.\n");
        return 1;
    }

    // Zeige mmls-Tabelle zur Kontrolle und warte auf ENTER
    printf("\n[Info] Ausgabe der Partitionstabelle (mmls):\n");
    system("clear");
    char mmls_cmd[600];
    snprintf(mmls_cmd, sizeof(mmls_cmd), "mmls %s", raw_image_path);
    system(mmls_cmd);
    printf("\n[Benutzereingabe] Bitte ENTER drücken, um mit der Entschlüsselung fortzufahren...");
    getchar();

    printf("BDP: Slot %d | Start: %lu | Länge: %lu Sektoren\n", bdp_info.slot, bdp_info.start, bdp_info.length);

    if (!run_dislocker(raw_image_path, bdp_info.start, bitlocker_key, bitlocker_path)) {
        fprintf(stderr, "Dislocker-Entschlüsselung fehlgeschlagen.\n");
        return 1;
    }

    char dislocker_file[512];
    snprintf(dislocker_file, sizeof(dislocker_file), "%s/dislocker-file", bitlocker_path);

    char loop_decrypted[64];
    char loop_original[64];

    if (!setup_loop_device(dislocker_file, loop_decrypted, sizeof(loop_decrypted))) {
        fprintf(stderr, "Loop-Device für entschlüsselten Bereich fehlgeschlagen.\n");
        return 1;
    }

    if (!setup_loop_device(raw_image_path, loop_original, sizeof(loop_original))) {
        fprintf(stderr, "Loop-Device für Original fehlgeschlagen.\n");
        return 1;
    }

    printf("Loop-Device (entschlüsselt): %s\n", loop_decrypted);
    printf("Loop-Device (original):      %s\n", loop_original);

    char mapping_file[512];
    snprintf(mapping_file, sizeof(mapping_file), "%s/dmsetup.txt", run_dir);

    if (!create_mapping_file(loop_original, loop_decrypted, bdp_info.start, bdp_info.length, mapping_file)) {
        fprintf(stderr, "Mapping-Datei konnte nicht erstellt werden.\n");
        return 1;
    }

    if (!setup_dm_device(mapping_file)) {
        fprintf(stderr, "dmsetup konnte das gemergte Gerät nicht erstellen.\n");
        return 1;
    }

    printf("\nVerarbeitung abgeschlossen. Ergebnisverzeichnis:\n");
    printf("  RAW-Image:           %s\n", raw_image_path);
    printf("  Entschlüsseltes File: %s\n", dislocker_file);
    printf("  Mapping-Datei:       %s\n", mapping_file);
    printf("  Loop (original):     %s\n", loop_original);
    printf("  Loop (decrypted):    %s\n", loop_decrypted);
    printf("  Mapper:              /dev/mapper/merged\n");

    return 0;
}
