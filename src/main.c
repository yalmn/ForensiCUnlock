#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include "forensic.h"
#include "mount_selector.h"
#include "image_converter.h"
#include "partition_parser.h"
#include "dislocker_runner.h"
#include "loop_device.h"
#include "mapper.h"
#include "imager.h"

int main(int argc, char *argv[]) {
    if (getenv("SUDO_USER") == NULL || strcmp(getenv("SUDO_USER"), "") == 0) {
        fprintf(stderr, "Fehler: Dieses Programm muss mit 'sudo' aufgerufen werden (nicht nur als root).\n");
        return 1;
    }
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <device> <bitlocker_key>\n", argv[0]);
        return 1;
    }

    if (geteuid() != 0) {
        fprintf(stderr, "Zugriff verweigert: Root-Rechte erforderlich.\n");
        return 1;
    }

    const char *device = argv[1];
    const char *bitlocker_key = argv[2];

    char selected_image_dir[512];
    char raw_image_path[512] = "/mnt/xmount/image.dd";

    printf("Mountvorgang gestartet für Gerät: %s\n", device);
    if (!auto_mount_and_find_ewf(device, selected_image_dir, sizeof(selected_image_dir))) {
        fprintf(stderr, "Fehler: Das EWF-Verzeichnis konnte nicht automatisch gefunden werden.\n");
        return 1;
    }

    printf("Gefundenes Verzeichnis: %s\n", selected_image_dir);

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
        printf("EWF-Datei erkannt. Starte Konvertierung nach RAW...\n");
        if (!convert_ewf_to_raw(selected_image_dir)) {
            fprintf(stderr, "Fehler: RAW-Image konnte nicht erstellt werden.\n");
            return 1;
        }
    } else {
        printf("Keine EWF-Datei erkannt. Verwende vorhandenes RAW-Image: %s\n", raw_image_path);
    }

    PartitionInfo bdp_info;
    if (!find_bdp_partition(raw_image_path, &bdp_info)) {
        fprintf(stderr, "Fehler: BDP konnte nicht erkannt werden.\n");
        return 1;
    }

    printf("BDP gefunden: Slot %d, Start %lu, Länge %lu\n",
           bdp_info.slot, (unsigned long)bdp_info.start, (unsigned long)bdp_info.length);

    if (!run_dislocker(raw_image_path, bdp_info.start, bitlocker_key)) {
        fprintf(stderr, "Fehler: Entschlüsselung mit dislocker fehlgeschlagen.\n");
        return 1;
    }

    char loop_decrypted[64];
    char loop_original[64];

    if (!setup_loop_device("/mnt/bitlocker/dislocker-file", loop_decrypted, sizeof(loop_decrypted))) {
        fprintf(stderr, "Fehler: Loop-Device für entschlüsselten Bereich konnte nicht erstellt werden.\n");
        return 1;
    }
    printf("Loop-Device (entschlüsselt): %s\n", loop_decrypted);

    if (!setup_loop_device(raw_image_path, loop_original, sizeof(loop_original))) {
        fprintf(stderr, "Fehler: Loop-Device für Original-Image konnte nicht erstellt werden.\n");
        return 1;
    }
    printf("Loop-Device (original): %s\n", loop_original);

    if (!create_mapping_file(loop_original, loop_decrypted, bdp_info.start, bdp_info.length)) {
        fprintf(stderr, "Fehler: Mapping-Datei konnte nicht erstellt werden.\n");
        return 1;
    }

    if (!setup_dm_device()) {
        fprintf(stderr, "Fehler: dmsetup konnte das gemergte Gerät nicht erstellen.\n");
        return 1;
    }

    if (!create_final_image()) {
        fprintf(stderr, "Fehler: Erstellung des finalen Images mit ddrescue fehlgeschlagen.\n");
        return 1;
    }

    printf("ForensiCUnlock erfolgreich abgeschlossen.\n");
    printf("\n[Übersicht der Ausgabedateien]\n");
    printf("  RAW-Image:                  /mnt/xmount/image.dd\n");
    printf("  Entschlüsselter Bereich:    /mnt/bitlocker/dislocker-file\n");
    printf("  Mapping-Datei:              /mnt/output/dmsetup.txt\n");
    printf("  Gemergtes Device:           /dev/mapper/merged\n");
    printf("  Finales Image (Output):     /mnt/output/merged_image.dd\n");
    printf("  Log-Datei ddrescue:         /mnt/output/merged_image.dd.log\n");

    return 0;
}
