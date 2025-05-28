#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "image_converter.h"
#include "partition_parser.h"
#include "dislocker_runner.h"
#include "loop_device.h"
#include "image_merger.h"

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
        fprintf(stderr, "Verwendung: %s <image|device> <bitlocker_key> <ausgabeordner>\n", argv[0]);
        return 1;
    }

    const char *input_image = argv[1];
    const char *bitlocker_key = argv[2];
    const char *output_folder = argv[3];

    char xmount_dir[512];
    char bitlocker_dir[512];
    char raw_image_path[1024];
    char dislocker_file[1024];

    snprintf(xmount_dir, sizeof(xmount_dir), "%s/xmount", output_folder);
    snprintf(bitlocker_dir, sizeof(bitlocker_dir), "%s/bitlocker", output_folder);

    printf("[*] Arbeitsverzeichnis: %s\n", output_folder);
    if (mkdir(output_folder, 0755) != 0 && errno != EEXIST)
    {
        perror("[!] Fehler beim Erstellen des Arbeitsverzeichnisses");
        return 1;
    }

    // Eingabeformat ermitteln
    const char *ext = get_extension(input_image);
    if (strcasecmp(ext, ".E01") == 0 || strcasecmp(ext, ".ewf") == 0)
    {
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
        printf("[*] RAW-Image erkannt: %s\n", input_image);
        strncpy(raw_image_path, input_image, sizeof(raw_image_path) - 1);
        raw_image_path[sizeof(raw_image_path) - 1] = '\0';
    }

    // Partitionserkennung
    printf("[*] Untersuche Partitionstabelle...\n");
    PartitionInfo bdp_info;
    if (!find_bdp_partition(raw_image_path, &bdp_info))
    {
        fprintf(stderr, "[!] Fehler: Basic Data Partition (BDP) konnte nicht erkannt werden.\n");
        return 1;
    }

    uint64_t bdp_end = bdp_info.start + bdp_info.length - 1;
    printf("[*] BDP erkannt:\n");
    printf("    → Startsektor : %llu\n", (unsigned long long)bdp_info.start);
    printf("    → Länge       : %llu Sektoren\n", (unsigned long long)bdp_info.length);
    printf("    → Endsektor   : %llu\n", (unsigned long long)bdp_end);

    // mmls zur Kontrolle anzeigen
    char mmls_cmd[2048];
    snprintf(mmls_cmd, sizeof(mmls_cmd), "mmls -i raw '%s'", raw_image_path);
    printf("\n[*] Partitionstabelle zur Kontrolle (mmls):\n");
    system(mmls_cmd);

    printf("\n[?] Prüfe die Angaben. Drücke ENTER zum Fortfahren...\n");
    getc(stdin);

    // Dislocker ausführen
    printf("[*] Starte Entschlüsselung mit Dislocker...\n");
    if (!run_dislocker(raw_image_path, bdp_info.start, bitlocker_key, bitlocker_dir))
    {
        fprintf(stderr, "[!] Fehler bei der BitLocker-Entschlüsselung.\n");
        return 1;
    }
    snprintf(dislocker_file, sizeof(dislocker_file), "%s/dislocker-file", bitlocker_dir);

    // Merge und Cleanup
    printf("[*] Erzeuge entschlüsseltes Abbild (merged.dd)...\n");
    if (!merge_and_cleanup(raw_image_path, dislocker_file, &bdp_info, output_folder))
    {
        fprintf(stderr, "[!] Fehler beim Zusammenführen der Datenbereiche.\n");
        return 1;
    }

    printf("\n[+] Vorgang abgeschlossen\n");
    printf("[+] Entschlüsseltes Image: \033[1;32m%s/merged.dd\033[0m\n", output_folder);
    return 0;
}
