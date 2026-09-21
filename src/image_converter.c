// src/image_converter.c
#include "image_converter.h"
#include "exec_utils.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

// Länge des Präfixes vor der zweistelligen Segmentnummer ("E" oder "Ex"), 0 bei unbekannter Endung
static size_t ewf_prefix_len(const char *ext)
{
    size_t len = strlen(ext);
    if (len == 3 && toupper((unsigned char)ext[0]) == 'E')
        return 1;
    if (len == 4 && toupper((unsigned char)ext[0]) == 'E' && tolower((unsigned char)ext[1]) == 'x')
        return 2;
    return 0;
}

// Segmentnummern sind entweder zwei Ziffern (01..99) oder zwei Buchstaben (AA..ZZ)
static int is_segment_pair(char a, char b)
{
    return (isdigit((unsigned char)a) && isdigit((unsigned char)b)) ||
           (isalpha((unsigned char)a) && isalpha((unsigned char)b));
}

int is_ewf_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || dot == path || (slash && dot < slash))
        return 0;

    const char *ext = dot + 1;
    if (strcasecmp(ext, "ewf") == 0)
        return 1;

    // Eingaben sind immer das erste Segment, daher reichen Ziffern (E01, Ex01, auch E02 für die Fehlermeldung)
    size_t prefix = ewf_prefix_len(ext);
    return prefix > 0 && isdigit((unsigned char)ext[prefix]) && isdigit((unsigned char)ext[prefix + 1]);
}

int ewf_segment_ext(const char *first_ext, int n, char *out, size_t len)
{
    size_t prefix = ewf_prefix_len(first_ext);
    if (prefix == 0 || n < 1 || len < prefix + 3)
        return 0;

    int lower = islower((unsigned char)first_ext[0]);
    char base = lower ? 'a' : 'A';
    memcpy(out, first_ext, prefix);

    if (n <= 99)
    {
        out[prefix] = (char)('0' + n / 10);
        out[prefix + 1] = (char)('0' + n % 10);
        out[prefix + 2] = '\0';
        return 1;
    }

    int idx = n - 100;
    if (prefix == 1)
    {
        // E01..E99, danach EAA..EZZ, FAA..FZZ usw. bis ZZZ
        int first = toupper((unsigned char)first_ext[0]) - 'A' + idx / 676;
        if (first > 25)
            return 0;
        out[0] = (char)(base + first);
        idx %= 676;
    }
    else if (idx >= 676)
    {
        return 0;
    }

    out[prefix] = (char)(base + idx / 26);
    out[prefix + 1] = (char)(base + idx % 26);
    out[prefix + 2] = '\0';
    return 1;
}

int check_ewf_segments(const char *first_segment, int *count)
{
    *count = 0;

    struct stat st;
    if (stat(first_segment, &st) != 0 || !S_ISREG(st.st_mode))
    {
        fprintf(stderr, "[!] EWF-Datei nicht gefunden: %s\n", first_segment);
        return 0;
    }

    if (!is_ewf_path(first_segment))
    {
        fprintf(stderr, "[!] Keine EWF-Endung: %s\n", first_segment);
        return 0;
    }
    const char *dot = strrchr(first_segment, '.');
    const char *ext = dot + 1;
    if (strcasecmp(ext, "ewf") == 0)
    {
        *count = 1;
        return 1;
    }

    size_t prefix = ewf_prefix_len(ext);
    if (prefix == 0 || ext[prefix] != '0' || ext[prefix + 1] != '1')
    {
        fprintf(stderr, "[!] Bitte das erste Segment angeben (.E01 bzw. .Ex01), nicht %s\n", first_segment);
        return 0;
    }

    // Verzeichnis und Dateiname trennen
    char dir[PATH_MAX];
    const char *slash = strrchr(first_segment, '/');
    if (slash)
        snprintf(dir, sizeof(dir), "%.*s", (int)(slash - first_segment), first_segment);
    else
        snprintf(dir, sizeof(dir), ".");
    if (dir[0] == '\0')
        snprintf(dir, sizeof(dir), "/");
    const char *name = slash ? slash + 1 : first_segment;
    size_t stem_len = (size_t)(dot - name) + 1 + prefix; // "disk.E" bzw. "disk.Ex"

    // Segmente der Reihe nach suchen, bis eines fehlt
    char path[PATH_MAX], seg_ext[8];
    int sequential = 0;
    for (int n = 1; n <= EWF_MAX_SEGMENTS; n++)
    {
        if (!ewf_segment_ext(ext, n, seg_ext, sizeof(seg_ext)))
            break;
        snprintf(path, sizeof(path), "%.*s%s", (int)(dot + 1 - first_segment), first_segment, seg_ext);
        if (access(path, R_OK) != 0)
            break;
        sequential = n;
    }

    // Alle Dateien mit passendem Namensmuster zählen, um Lücken zu erkennen
    DIR *d = opendir(dir);
    if (!d)
    {
        perror("[!] Verzeichnis der EWF-Segmente nicht lesbar");
        return 0;
    }
    int total = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (strlen(entry->d_name) != stem_len + 2)
            continue;
        if (strncmp(entry->d_name, name, stem_len - prefix) != 0)
            continue;
        if (strncasecmp(entry->d_name + stem_len - prefix, ext, prefix) != 0)
            continue;
        if (is_segment_pair(entry->d_name[stem_len], entry->d_name[stem_len + 1]))
            total++;
    }
    closedir(d);

    if (total > sequential)
    {
        ewf_segment_ext(ext, sequential + 1, seg_ext, sizeof(seg_ext));
        fprintf(stderr, "[!] EWF-Image unvollständig: Segment .%s fehlt (%d Segmente zusammenhängend, %d vorhanden).\n",
                seg_ext, sequential, total);
        return 0;
    }

    *count = sequential;
    return 1;
}

int mount_ewf(const char *first_segment, const char *mount_dir, char *raw_path, size_t len)
{
    int segments;
    if (!check_ewf_segments(first_segment, &segments))
        return 0;
    printf("[*] EWF-Image mit %d Segment(en) gefunden.\n", segments);

    if (!make_dir(mount_dir))
    {
        fprintf(stderr, "[!] Mountverzeichnis %s konnte nicht angelegt werden.\n", mount_dir);
        return 0;
    }

    printf("[*] Hänge EWF-Image mit ewfmount ein...\n");
    char *argv[] = {"ewfmount", (char *)first_segment, (char *)mount_dir, NULL};
    if (!run_cmd(argv))
    {
        fprintf(stderr, "[!] ewfmount ist fehlgeschlagen.\n");
        return 0;
    }

    if (snprintf(raw_path, len, "%s/ewf1", mount_dir) >= (int)len || access(raw_path, R_OK) != 0)
    {
        fprintf(stderr, "[!] RAW-Sicht nicht gefunden unter %s/ewf1\n", mount_dir);
        return 0;
    }
    return 1;
}
