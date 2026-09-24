// src/dislocker_runner.c
#include "dislocker_runner.h"
#include "exec_utils.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int build_key_args(KeyMode mode, const char *key, const char *out[2], char **heap_arg)
{
    *heap_arg = NULL;
    if (!key)
        return 0;

    if (mode == KEY_VMK)
    {
        // dislocker liest den VMK aus der Datei hinter -K
        out[0] = "-K";
        out[1] = key;
        return 2;
    }

    // KEY_RECOVERY: dislocker erwartet den Schlüssel direkt hinter -p
    size_t len = strlen(key) + 3;
    char *arg = malloc(len);
    if (!arg)
        return 0;
    snprintf(arg, len, "-p%s", key);
    *heap_arg = arg;
    out[0] = arg;
    return 1;
}

int run_dislocker(const char *image_path, uint64_t offset_bytes, KeyMode mode, const char *key, const char *output_dir)
{
    if (!make_dir(output_dir))
    {
        fprintf(stderr, "[!] Verzeichnis %s konnte nicht angelegt werden.\n", output_dir);
        return 0;
    }

    char offset[32];
    snprintf(offset, sizeof(offset), "%" PRIu64, offset_bytes);

    const char *key_args[2];
    char *heap_arg = NULL;
    int key_n = build_key_args(mode, key, key_args, &heap_arg);
    if (key_n == 0)
        return 0;

    // argv fest zusammensetzen: dislocker -V <img> -O <off> <schlüssel...> -r -- <dir>
    char *argv[11];
    int i = 0;
    argv[i++] = "dislocker";
    argv[i++] = "-V";
    argv[i++] = (char *)image_path;
    argv[i++] = "-O";
    argv[i++] = offset;
    for (int k = 0; k < key_n; k++)
        argv[i++] = (char *)key_args[k];
    argv[i++] = "-r";
    argv[i++] = "--";
    argv[i++] = (char *)output_dir;
    argv[i] = NULL;

    // Den Schlüssel nicht ins Log schreiben. Bei VMK ist der Dateipfad unkritisch.
    if (mode == KEY_VMK)
        printf("[*] dislocker -V '%s' -O %s -K '%s' -r -- '%s'\n", image_path, offset, key, output_dir);
    else
        printf("[*] dislocker -V '%s' -O %s -p<schlüssel> -r -- '%s'\n", image_path, offset, output_dir);

    int ok = run_cmd(argv);

    if (heap_arg)
    {
        memset(heap_arg, 0, strlen(heap_arg));
        free(heap_arg);
    }

    char file[4096];
    snprintf(file, sizeof(file), "%s/dislocker-file", output_dir);
    if (!ok || access(file, R_OK) != 0)
    {
        fprintf(stderr, "[!] dislocker konnte das Volume nicht öffnen. Mögliche Ursachen: "
                        "falscher Schlüssel ODER ein von dislocker nicht unterstütztes Volume "
                        "(z. B. EOW / neuere BitLocker-Variante). Details siehe dislocker-Ausgabe oben.\n");
        return 0;
    }
    return 1;
}

int dump_dislocker_metadata(const char *image_path, uint64_t offset_bytes, const char *out_file)
{
    char offset[32];
    snprintf(offset, sizeof(offset), "%" PRIu64, offset_bytes);

    // dislocker-metadata -V <volume> -o <offset>  (kleines -o, kein Schlüssel nötig)
    char *argv[] = {"dislocker-metadata", "-V", (char *)image_path, "-o", offset, NULL};

    pid_t pid;
    FILE *fp = run_cmd_read(argv, &pid);
    if (!fp)
    {
        fprintf(stderr, "[!] dislocker-metadata konnte nicht gestartet werden.\n");
        return 0;
    }

    FILE *out = fopen(out_file, "w");
    if (!out)
    {
        perror("[!] Metadaten-Datei konnte nicht geschrieben werden");
        // Stream leeren, damit das Kind nicht auf eine volle Pipe blockiert
        char drain[4096];
        while (fread(drain, 1, sizeof(drain), fp) > 0)
            ;
        close_cmd_read(fp, pid);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        fwrite(buf, 1, n, out);
    fclose(out);

    return close_cmd_read(fp, pid);
}
