// src/dislocker_runner.c
#include "dislocker_runner.h"
#include "exec_utils.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int run_dislocker(const char *image_path, uint64_t offset_bytes, const char *key, const char *output_dir)
{
    if (!make_dir(output_dir))
    {
        fprintf(stderr, "[!] Verzeichnis %s konnte nicht angelegt werden.\n", output_dir);
        return 0;
    }

    char offset[32];
    snprintf(offset, sizeof(offset), "%" PRIu64, offset_bytes);

    // dislocker erwartet den Schlüssel direkt hinter -p
    size_t key_len = strlen(key) + 3;
    char *key_arg = malloc(key_len);
    if (!key_arg)
        return 0;
    snprintf(key_arg, key_len, "-p%s", key);

    char *argv[] = {"dislocker", "-V", (char *)image_path, "-O", offset, key_arg, "-r", "--", (char *)output_dir, NULL};

    // Den Schlüssel nicht ins Log schreiben
    printf("[*] dislocker -V '%s' -O %s -p<schlüssel> -r -- '%s'\n", image_path, offset, output_dir);
    int ok = run_cmd(argv);

    memset(key_arg, 0, key_len);
    free(key_arg);

    char file[4096];
    snprintf(file, sizeof(file), "%s/dislocker-file", output_dir);
    if (!ok || access(file, R_OK) != 0)
    {
        fprintf(stderr, "[!] dislocker konnte das Volume nicht entschlüsseln (falscher Schlüssel?).\n");
        return 0;
    }
    return 1;
}
