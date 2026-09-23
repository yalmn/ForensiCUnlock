// src/main.c
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "dislocker_runner.h"
#include "exec_utils.h"
#include "image_converter.h"
#include "image_merger.h"
#include "ova_mounter.h"
#include "partition_parser.h"

static volatile sig_atomic_t interrupted = 0;

static void on_signal(int sig)
{
    (void)sig;
    interrupted = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    // Kein SA_RESTART, damit wartende Eingaben bei Ctrl+C sofort abbrechen
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

static void usage(const char *prog)
{
    fprintf(stderr, "Verwendung: %s <image|device> <recovery-key> [ausgabeordner]\n", prog);
    fprintf(stderr, "        oder %s <image|device> --vmk <vmk-datei> [ausgabeordner]\n", prog);
    fprintf(stderr, "  image          RAW-Image (.dd/.raw/.img), EWF-Image (.E01 bzw. .Ex01),\n");
    fprintf(stderr, "                 VM-Export (.ova) oder Blockgerät\n");
    fprintf(stderr, "  recovery-key   BitLocker-Wiederherstellungsschlüssel (48 Ziffern)\n");
    fprintf(stderr, "  --vmk <datei>  Datei mit dem Volume Master Key (z. B. aus einem TPM-Sniff)\n");
    fprintf(stderr, "  ausgabeordner  optional, sonst ./run_JJJJMMTT_HHMMSS\n");
}

// Liest eine Zeile von stdin. Liefert 0 bei EOF oder Abbruch.
static int read_line(char *buf, size_t len)
{
    fflush(stdout);
    if (!fgets(buf, (int)len, stdin) || interrupted)
        return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static void print_partition(int index, const PartitionInfo *p)
{
    printf("    [%d] Slot %03d: Start %" PRIu64 ", Ende %" PRIu64 ", Länge %" PRIu64 " Sektoren (%" PRIu32 " Byte)%s%s\n",
           index, p->slot, p->start, p->start + p->length - 1, p->length, p->sector_size,
           p->description[0] ? ", " : "", p->description);
}

static int select_partition(const PartitionInfo *list, int count, PartitionInfo *selected)
{
    if (count == 1)
    {
        *selected = list[0];
        return 1;
    }

    printf("[?] Mehrere BitLocker-Partitionen gefunden:\n");
    for (int i = 0; i < count; i++)
        print_partition(i + 1, &list[i]);

    char line[32];
    while (1)
    {
        printf("[?] Welche Partition soll entschlüsselt werden? (1-%d): ", count);
        if (!read_line(line, sizeof(line)))
            return 0;
        char *end;
        long choice = strtol(line, &end, 10);
        if (end != line && *end == '\0' && choice >= 1 && choice <= count)
        {
            *selected = list[choice - 1];
            return 1;
        }
        printf("[!] Ungültige Eingabe.\n");
    }
}

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int main(int argc, char *argv[])
{
    // Zeilenweise ausgeben, damit Logs (z. B. mit tee) sofort aktuell sind
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 3 || argc > 5)
    {
        usage(argv[0]);
        return 1;
    }
    if (geteuid() != 0)
    {
        fprintf(stderr, "[!] Fehler: Bitte führe das Programm mit root-Rechten aus (sudo).\n");
        return 1;
    }

    const char *input_image = argv[1];
    KeyMode key_mode;
    const char *key;
    int out_idx; // Index des optionalen Ausgabeordners

    if (strcmp(argv[2], "--vmk") == 0)
    {
        // Zweite Form: <image> --vmk <datei> [ausgabeordner]
        if (argc < 4)
        {
            usage(argv[0]);
            return 1;
        }
        key_mode = KEY_VMK;
        key = argv[3];
        out_idx = 4;
    }
    else
    {
        // Erste Form: <image> <recovery-key> [ausgabeordner]
        key_mode = KEY_RECOVERY;
        key = argv[2];
        out_idx = 3;
    }

    // Nach dem Schlüssel darf höchstens noch der Ausgabeordner folgen.
    if (argc > out_idx + 1)
    {
        usage(argv[0]);
        return 1;
    }

    char output_folder[PATH_MAX];
    if (argc == out_idx + 1)
    {
        snprintf(output_folder, sizeof(output_folder), "%s", argv[out_idx]);
    }
    else
    {
        time_t now = time(NULL);
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", localtime(&now));
        snprintf(output_folder, sizeof(output_folder), "run_%s", stamp);
    }

    // Abschließende Schrägstriche entfernen, damit keine Pfade wie "ordner//ova" entstehen
    size_t out_len = strlen(output_folder);
    while (out_len > 1 && output_folder[out_len - 1] == '/')
        output_folder[--out_len] = '\0';

    char ewf_dir[PATH_MAX + 16], bitlocker_dir[PATH_MAX + 16], merged_path[PATH_MAX + 16];
    char info_path[PATH_MAX + 16], dislocker_file[PATH_MAX + 32], raw_image_path[PATH_MAX + 16];
    char ova_dir[PATH_MAX + 16];
    snprintf(ewf_dir, sizeof(ewf_dir), "%s/ewf", output_folder);
    snprintf(bitlocker_dir, sizeof(bitlocker_dir), "%s/bitlocker", output_folder);
    snprintf(merged_path, sizeof(merged_path), "%s/merged.dd", output_folder);
    snprintf(info_path, sizeof(info_path), "%s/bdp.info", output_folder);
    snprintf(dislocker_file, sizeof(dislocker_file), "%s/dislocker-file", bitlocker_dir);
    snprintf(ova_dir, sizeof(ova_dir), "%s/ova", output_folder);

    // Zum Aufräumen: per qemu-nbd verbundene Blockgeräte aus einer OVA
    char nbd_devices[OVA_MAX_DISKS][64];
    int nbd_count = 0;

    if (!path_exists(input_image))
    {
        fprintf(stderr, "[!] Eingabe nicht gefunden: %s\n", input_image);
        return 1;
    }
    if (key_mode == KEY_VMK && !path_exists(key))
    {
        fprintf(stderr, "[!] VMK-Datei nicht gefunden: %s\n", key);
        return 1;
    }
    if (path_exists(merged_path))
    {
        fprintf(stderr, "[!] %s existiert bereits. Bitte einen anderen Ausgabeordner wählen.\n", merged_path);
        return 1;
    }
    if (is_mountpoint(ewf_dir) || is_mountpoint(bitlocker_dir))
    {
        fprintf(stderr, "[!] Im Ausgabeordner ist noch etwas eingehängt (%s oder %s). Bitte zuerst aushängen.\n",
                ewf_dir, bitlocker_dir);
        return 1;
    }

    int created_output = !path_exists(output_folder);
    printf("[*] Arbeitsverzeichnis: %s\n", output_folder);
    if (!make_dir(output_folder))
    {
        perror("[!] Fehler beim Erstellen des Arbeitsverzeichnisses");
        return 1;
    }

    install_signal_handlers();
    int success = 0;
    PartitionInfo bdp_info;

    // Eingabeformat ermitteln
    int have_selection = 0; // bei OVA werden Disk und Partition direkt hier gewählt
    if (is_ova_path(input_image))
    {
        printf("[*] OVA-Datei erkannt: %s\n", input_image);
        OvaDisk disks[OVA_MAX_DISKS];
        int disk_count = extract_ova_disks(input_image, ova_dir, disks, OVA_MAX_DISKS);
        if (disk_count < 0)
            goto cleanup;
        printf("[*] %d Disk(s) in der OVA.\n", disk_count);

        // Alle Disks einhängen und nach BitLocker-Partitionen durchsuchen.
        // Kandidaten werden über alle Disks hinweg gesammelt (Gerät + Partition).
        char cand_dev[MAX_PARTITIONS][64];
        char cand_label[MAX_PARTITIONS][160];
        PartitionInfo cand_part[MAX_PARTITIONS];
        int cand_count = 0;

        for (int i = 0; i < disk_count && !interrupted; i++)
        {
            char dev[64];
            printf("[*] Hänge Disk %d/%d ein (%s)...\n", i + 1, disk_count, disks[i].label);
            if (!nbd_connect(disks[i].vmdk_path, dev, sizeof(dev)))
                goto cleanup;
            snprintf(nbd_devices[nbd_count++], sizeof(nbd_devices[0]), "%s", dev);

            PartitionInfo parts[MAX_PARTITIONS];
            int c = find_bitlocker_partitions(dev, parts, MAX_PARTITIONS);
            if (c < 0)
                goto cleanup;
            for (int j = 0; j < c && cand_count < MAX_PARTITIONS; j++)
            {
                snprintf(cand_dev[cand_count], sizeof(cand_dev[0]), "%s", dev);
                snprintf(cand_label[cand_count], sizeof(cand_label[0]), "%s", disks[i].label);
                cand_part[cand_count] = parts[j];
                cand_count++;
            }
        }
        if (interrupted)
            goto cleanup;
        if (cand_count == 0)
        {
            fprintf(stderr, "[!] Keine BitLocker-Partition in der OVA gefunden.\n");
            goto cleanup;
        }

        int chosen = 0;
        if (cand_count > 1)
        {
            printf("[?] Mehrere BitLocker-Partitionen in der OVA gefunden:\n");
            for (int k = 0; k < cand_count; k++)
                printf("    [%d] Disk %s | Slot %03d: Start %" PRIu64 ", Länge %" PRIu64 " Sektoren (%" PRIu32 " Byte)%s%s\n",
                       k + 1, cand_label[k], cand_part[k].slot, cand_part[k].start, cand_part[k].length,
                       cand_part[k].sector_size, cand_part[k].description[0] ? ", " : "", cand_part[k].description);

            char line[32];
            while (1)
            {
                printf("[?] Welche soll entschlüsselt werden? (1-%d): ", cand_count);
                if (!read_line(line, sizeof(line)))
                    goto cleanup;
                char *end;
                long choice = strtol(line, &end, 10);
                if (end != line && *end == '\0' && choice >= 1 && choice <= cand_count)
                {
                    chosen = (int)choice - 1;
                    break;
                }
                printf("[!] Ungültige Eingabe.\n");
            }
        }
        snprintf(raw_image_path, sizeof(raw_image_path), "%s", cand_dev[chosen]);
        bdp_info = cand_part[chosen];
        have_selection = 1;
    }
    else if (is_ewf_path(input_image))
    {
        printf("[*] EWF-Image erkannt: %s\n", input_image);
        if (!mount_ewf(input_image, ewf_dir, raw_image_path, sizeof(raw_image_path)))
            goto cleanup;
    }
    else
    {
        printf("[*] RAW-Image bzw. Blockgerät erkannt: %s\n", input_image);
        snprintf(raw_image_path, sizeof(raw_image_path), "%s", input_image);
    }
    if (interrupted)
        goto cleanup;

    // Partitionserkennung (bei OVA bereits oben erledigt)
    if (!have_selection)
    {
        printf("[*] Suche BitLocker-Partitionen...\n");
        PartitionInfo partitions[MAX_PARTITIONS];
        int count = find_bitlocker_partitions(raw_image_path, partitions, MAX_PARTITIONS);
        if (count < 0)
            goto cleanup;
        if (count == 0)
        {
            fprintf(stderr, "[!] Keine BitLocker-Partition gefunden.\n");
            goto cleanup;
        }
        if (!select_partition(partitions, count, &bdp_info))
            goto cleanup;
    }

    printf("[*] BitLocker-Partition:\n");
    printf("    → Slot        : %03d\n", bdp_info.slot);
    printf("    → Startsektor : %" PRIu64 "\n", bdp_info.start);
    printf("    → Länge       : %" PRIu64 " Sektoren\n", bdp_info.length);
    printf("    → Endsektor   : %" PRIu64 "\n", bdp_info.start + bdp_info.length - 1);
    printf("    → Sektorgröße : %" PRIu32 " Byte\n", bdp_info.sector_size);

    printf("\n[*] Partitionstabelle zur Kontrolle (mmls):\n");
    char *mmls_argv[] = {"mmls", "-i", "raw", raw_image_path, NULL};
    run_cmd(mmls_argv);

    printf("\n[?] Prüfe die Angaben. ENTER zum Fortfahren, Ctrl+C zum Abbrechen...");
    char answer[16];
    if (!read_line(answer, sizeof(answer)))
    {
        printf("\n");
        goto cleanup;
    }

    if (!write_partition_info(info_path, &bdp_info))
    {
        perror("[!] bdp.info konnte nicht geschrieben werden");
        goto cleanup;
    }

    // Entschlüsselung
    printf("[*] Starte Entschlüsselung mit dislocker...\n");
    if (!run_dislocker(raw_image_path, bdp_info.start * bdp_info.sector_size, key_mode, key, bitlocker_dir))
        goto cleanup;
    if (interrupted)
        goto cleanup;

    // Zusammenführen
    printf("[*] Erzeuge entschlüsseltes Abbild (merged.dd)...\n");
    if (!merge_image(raw_image_path, dislocker_file, &bdp_info, merged_path, &interrupted))
    {
        fprintf(stderr, "[!] Fehler beim Zusammenführen der Datenbereiche.\n");
        goto cleanup;
    }
    success = 1;

cleanup:
    if (interrupted)
        fprintf(stderr, "\n[!] Abgebrochen, räume auf...\n");
    else
        printf("[*] Bereinige temporäre Dateien...\n");

    int cleaned = 1;
    if (is_mountpoint(bitlocker_dir))
        cleaned &= unmount_fuse(bitlocker_dir);
    if (is_mountpoint(ewf_dir))
        cleaned &= unmount_fuse(ewf_dir);
    rmdir(bitlocker_dir);
    rmdir(ewf_dir);

    // OVA: erst die nbd-Geräte lösen (dislocker las von dort), dann die
    // extrahierten VMDKs entfernen. Sie sind nur ein Zwischenschritt und
    // lassen sich jederzeit erneut aus der OVA gewinnen.
    for (int i = 0; i < nbd_count; i++)
        nbd_disconnect(nbd_devices[i]);
    if (path_exists(ova_dir))
    {
        char *rm_argv[] = {"rm", "-rf", ova_dir, NULL};
        run_cmd(rm_argv);
    }

    if (!success)
    {
        unlink(info_path);
        if (created_output)
            rmdir(output_folder);
    }
    if (!cleaned)
        fprintf(stderr, "[!] Nicht alle Mounts konnten ausgehängt werden, bitte manuell prüfen (mount | grep %s).\n",
                output_folder);

    if (!success)
    {
        fprintf(stderr, "[!] Vorgang fehlgeschlagen.\n");
        return interrupted ? 130 : 1;
    }

    printf("\n[+] Vorgang abgeschlossen\n");
    printf("[+] Entschlüsseltes Image: \033[1;32m%s\033[0m\n", merged_path);
    return cleaned ? 0 : 1;
}
