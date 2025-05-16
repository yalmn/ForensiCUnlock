#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "image_converter.h"
#include "partition_parser.h"
#include "dislocker_runner.h"
#include "loop_device.h"
#include "mapper.h"

// Helfer: Dateiendung extrahieren
static const char *get_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return (dot && dot != path) ? dot : "";
}

int main(int argc, char *argv[])
{
    if (geteuid() != 0 || getenv("SUDO_USER") == NULL)
    {
        fprintf(stderr, "[!] Fehler: Bitte führe das Programm mit 'sudo' aus.\n");
        return 1;
    }

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <device|image> <bitlocker_key> <output_folder>\n", argv[0]);
        return 1;
    }

    const char *input_image = argv[1];
    const char *bitlocker_key = argv[2];
    const char *output_folder = argv[3];

    char xmount_dir[512];
    char bitlocker_dir[512];
    char raw_image_path[1024];
    char dislocker_file[1024];
    char loop_decrypted[64];
    char loop_original[64];

    // Ausgabepfade setzen
    snprintf(xmount_dir, sizeof(xmount_dir), "%s/xmount", output_folder);
    snprintf(bitlocker_dir, sizeof(bitlocker_dir), "%s/bitlocker", output_folder);

    // Arbeitsverzeichnis anlegen
    printf("[*] Arbeitsordner: %s\n", output_folder);
    if (mkdir(output_folder, 0755) != 0 && errno != EEXIST)
    {
        perror("[!] Fehler beim Anlegen des Arbeitsordners");
        return 1;
    }

    // EWF-Container oder RAW-Image unterscheiden
    const char *ext = get_extension(input_image);
    if (strcasecmp(ext, ".E01") == 0 || strcasecmp(ext, ".ewf") == 0)
    {
        // EWF → RAW
        printf("[*] EWF-Image erkannt: %s\n", input_image);
        if (!convert_ewf_to_raw(input_image, xmount_dir))
        {
            fprintf(stderr, "[!] Fehler: EWF-Konvertierung fehlgeschlagen.\n");
            return 1;
        }
        snprintf(raw_image_path, sizeof(raw_image_path), "%s/image.dd", xmount_dir);
    }
    else
    {
        // RAW direkt verwenden
        printf("[*] RAW-Image erkannt: %s\n", input_image);
        strncpy(raw_image_path, input_image, sizeof(raw_image_path) - 1);
        raw_image_path[sizeof(raw_image_path) - 1] = '\0';
    }

    // Partitionserkennung
    printf("[*] Analysiere Partitionstabelle...\n");
    PartitionInfo bdp_info;
    if (!find_bdp_partition(raw_image_path, &bdp_info))
    {
        fprintf(stderr, "[!] Fehler: BDP-Partition nicht gefunden.\n");
        return 1;
    }
    printf("[*] BDP automatisch erkannt: Slot=%d, Start=%llu, Länge=%llu Sektoren\n",
           bdp_info.slot,
           (unsigned long long)bdp_info.start,
           (unsigned long long)bdp_info.length);

    // Kontroll-Ausgabe und mmls-Tabelle zur Verifikation
    printf("[*] Zu prüfender Bereich → Offset: %llu Sektoren, Länge: %llu Sektoren\n",
           (unsigned long long)bdp_info.start,
           (unsigned long long)bdp_info.length);
    {
        char mmls_cmd[2048];
        snprintf(mmls_cmd, sizeof(mmls_cmd), "mmls -i raw %s", raw_image_path);
        printf("[*] mmls-Partitionstabelle:\n");
        system(mmls_cmd);
    }
    printf("[*] ENTER zum Fortfahren...\n");
    system("read -p ''");

    // BitLocker-Entschlüsselung
    printf("[*] Starte BitLocker-Entschlüsselung...\n");
    if (!run_dislocker(raw_image_path, bdp_info.start, bitlocker_key, bitlocker_dir))
    {
        fprintf(stderr, "[!] Fehler bei der BitLocker-Entschlüsselung.\n");
        return 1;
    }
    snprintf(dislocker_file, sizeof(dislocker_file), "%s/dislocker-file", bitlocker_dir);

    // Loop-Devices erstellen
    if (!setup_loop_device(dislocker_file, loop_decrypted, sizeof(loop_decrypted)))
    {
        fprintf(stderr, "[!] Fehler beim Einrichten des Loop-Devices (entschlüsselt).\n");
        return 1;
    }
    if (!setup_loop_device(raw_image_path, loop_original, sizeof(loop_original)))
    {
        fprintf(stderr, "[!] Fehler beim Einrichten des Loop-Devices (original).\n");
        return 1;
    }
    printf("[*] Loop-Device entschlüsselt: %s\n", loop_decrypted);
    printf("[*] Loop-Device original:      %s\n", loop_original);

    // Mapping und Device-Mapper
    if (!create_mapping_file(loop_original, loop_decrypted,
                             bdp_info.start, bdp_info.length,
                             output_folder))
    {
        fprintf(stderr, "[!] Fehler beim Schreiben der Mapping-Datei.\n");
        return 1;
    }
    if (!setup_dm_device(output_folder))
    {
        fprintf(stderr, "[!] Fehler beim Erstellen des gemergten Devices.\n");
        return 1;
    }

    // Abschluss
    printf("[✔] Vorgang abgeschlossen. Gemergtes Device: /dev/mapper/merged\n");
    printf("Arbeitsverzeichnis: %s\n", output_folder);
    return 0;
}
