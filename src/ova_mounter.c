// src/ova_mounter.c
#include "ova_mounter.h"
#include "exec_utils.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

int is_ova_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || dot == path || (slash && dot < slash))
        return 0;
    return strcasecmp(dot + 1, "ova") == 0;
}

// Prüft, ob ein Programm über den PATH ausführbar ist, ohne es zu starten.
// So lässt sich eine fehlende Abhängigkeit einmal klar melden, statt bei jedem
// Startversuch die gleiche execvp-Fehlermeldung zu erzeugen.
static int program_available(const char *name)
{
    const char *path = getenv("PATH");
    if (!path || !*path)
        path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";

    char buf[PATH_MAX];
    for (const char *p = path; *p;)
    {
        const char *colon = strchr(p, ':');
        size_t dirlen = colon ? (size_t)(colon - p) : strlen(p);
        if (dirlen > 0 && dirlen + 1 + strlen(name) + 1 <= sizeof(buf))
        {
            memcpy(buf, p, dirlen);
            buf[dirlen] = '/';
            snprintf(buf + dirlen + 1, sizeof(buf) - dirlen - 1, "%s", name);
            if (access(buf, X_OK) == 0)
                return 1;
        }
        if (!colon)
            break;
        p = colon + 1;
    }
    return 0;
}

// Prüft, ob unter /sys/block/nbdN/pid ein aktiver qemu-nbd-Prozess hängt.
static int nbd_in_use(int n)
{
    char p[64];
    snprintf(p, sizeof(p), "/sys/block/nbd%d/pid", n);
    return access(p, F_OK) == 0;
}

// Stellt sicher, dass das nbd-Kernelmodul geladen ist (mindestens /dev/nbd0 existiert).
static int nbd_module_ready(void)
{
    if (access("/dev/nbd0", F_OK) == 0)
        return 1;
    printf("[*] Lade nbd-Kernelmodul...\n");
    char *argv[] = {"modprobe", "nbd", "max_part=16", NULL};
    run_cmd(argv);
    return access("/dev/nbd0", F_OK) == 0;
}

// Wartet, bis das Blockgerät bereit ist und eine Größe > 0 meldet.
static int wait_device_ready(const char *dev)
{
    for (int t = 0; t < 50; t++)
    {
        int fd = open(dev, O_RDONLY);
        if (fd >= 0)
        {
            off_t size = lseek(fd, 0, SEEK_END);
            close(fd);
            if (size > 0)
                return 1;
        }
        struct timespec ts = {0, 100 * 1000 * 1000}; // 100 ms
        nanosleep(&ts, NULL);
    }
    return 0;
}

int nbd_disconnect(const char *device)
{
    char *argv[] = {"qemu-nbd", "-d", (char *)device, NULL};
    return run_cmd(argv);
}

int nbd_connect(const char *vmdk_path, char *device_out, size_t len)
{
    if (!program_available("qemu-nbd"))
    {
        fprintf(stderr, "[!] 'qemu-nbd' nicht gefunden. Bitte qemu-utils installieren "
                        "(sudo apt install qemu-utils) oder ./scripts/install.sh ausführen.\n");
        return 0;
    }
    if (!nbd_module_ready())
    {
        fprintf(stderr, "[!] nbd-Kernelmodul nicht verfügbar (modprobe nbd fehlgeschlagen).\n");
        return 0;
    }

    for (int i = 0; i < 64; i++)
    {
        char dev[64];
        snprintf(dev, sizeof(dev), "/dev/nbd%d", i);
        if (access(dev, F_OK) != 0)
            break; // keine weiteren nbd-Geräte vorhanden
        if (nbd_in_use(i))
            continue;

        // read-only (-r) einhängen, Format explizit als vmdk vorgeben (-f vmdk)
        char *argv[] = {"qemu-nbd", "-r", "-f", "vmdk", "-c", dev, (char *)vmdk_path, NULL};
        if (!run_cmd(argv))
            continue; // Gerät evtl. doch belegt, nächstes versuchen

        if (wait_device_ready(dev))
        {
            if (snprintf(device_out, len, "%s", dev) >= (int)len)
            {
                nbd_disconnect(dev);
                return 0;
            }
            printf("[*] VMDK read-only eingehängt: %s -> %s\n", vmdk_path, dev);
            return 1;
        }
        // Gerät nicht bereit geworden -> wieder lösen und weiter
        nbd_disconnect(dev);
    }

    fprintf(stderr, "[!] Kein freies /dev/nbdX für %s gefunden.\n", vmdk_path);
    return 0;
}

int extract_ova_disks(const char *ova_path, const char *work_dir, OvaDisk *list, int max)
{
    if (max > OVA_MAX_DISKS)
        max = OVA_MAX_DISKS;

    if (!make_dir(work_dir))
    {
        fprintf(stderr, "[!] Arbeitsverzeichnis %s konnte nicht angelegt werden.\n", work_dir);
        return -1;
    }

    // Archivinhalt auflisten und VMDK-Mitglieder einsammeln
    char *list_argv[] = {"tar", "-tf", (char *)ova_path, NULL};
    pid_t pid;
    FILE *fp = run_cmd_read(list_argv, &pid);
    if (!fp)
    {
        fprintf(stderr, "[!] OVA konnte nicht gelesen werden (tar): %s\n", ova_path);
        return -1;
    }

    char members[OVA_MAX_DISKS][PATH_MAX];
    int found = 0;
    char line[PATH_MAX];
    while (fgets(line, sizeof(line), fp))
    {
        line[strcspn(line, "\r\n")] = '\0';
        size_t l = strlen(line);
        if (l < 5 || strcasecmp(line + l - 5, ".vmdk") != 0)
            continue;
        if (found < OVA_MAX_DISKS)
            snprintf(members[found++], PATH_MAX, "%s", line);
    }
    // Rückgabewert von tar ist unkritisch: Inhalt haben wir bereits gelesen.
    close_cmd_read(fp, pid);

    if (found == 0)
    {
        fprintf(stderr, "[!] Keine VMDK-Disk in der OVA gefunden: %s\n", ova_path);
        return -1;
    }
    if (found > max)
    {
        fprintf(stderr, "[!] OVA enthält %d Disks, es werden nur die ersten %d verarbeitet.\n", found, max);
        found = max;
    }

    // Ausgewählte Mitglieder gezielt extrahieren
    int out = 0;
    for (int i = 0; i < found; i++)
    {
        printf("[*] Extrahiere Disk aus OVA: %s\n", members[i]);
        char *ex_argv[] = {"tar", "-xf", (char *)ova_path, "-C", (char *)work_dir, members[i], NULL};
        if (!run_cmd(ex_argv))
        {
            fprintf(stderr, "[!] Extraktion fehlgeschlagen: %s\n", members[i]);
            return -1;
        }

        if (snprintf(list[out].vmdk_path, sizeof(list[out].vmdk_path), "%s/%s", work_dir, members[i]) >= (int)sizeof(list[out].vmdk_path))
        {
            fprintf(stderr, "[!] Pfad zur extrahierten VMDK zu lang: %s/%s\n", work_dir, members[i]);
            return -1;
        }
        if (access(list[out].vmdk_path, R_OK) != 0)
        {
            fprintf(stderr, "[!] Extrahierte VMDK nicht gefunden: %s\n", list[out].vmdk_path);
            return -1;
        }

        const char *base = strrchr(members[i], '/');
        snprintf(list[out].label, sizeof(list[out].label), "%s", base ? base + 1 : members[i]);
        out++;
    }
    return out;
}
