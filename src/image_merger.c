// src/image_merger.c
#include "image_merger.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define COPY_BUFFER_SIZE (1024 * 1024)

static int get_fd_size(int fd, uint64_t *size)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        return 0;

    if (S_ISREG(st.st_mode))
    {
        *size = (uint64_t)st.st_size;
        return 1;
    }
    if (S_ISBLK(st.st_mode))
        return ioctl(fd, BLKGETSIZE64, size) == 0;
    return 0;
}

int get_image_size(const char *path, uint64_t *size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    int ok = get_fd_size(fd, size);
    close(fd);
    return ok;
}

static int is_zero(const unsigned char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (buf[i] != 0)
            return 0;
    }
    return 1;
}

static int write_all(int fd, const unsigned char *buf, size_t len)
{
    while (len > 0)
    {
        ssize_t n = write(fd, buf, len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return 0;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 1;
}

int copy_range(int in_fd, uint64_t offset, uint64_t len, int out_fd, const volatile sig_atomic_t *stop)
{
    unsigned char *buf = malloc(COPY_BUFFER_SIZE);
    if (!buf)
        return 0;

    int ok = 1;
    while (len > 0)
    {
        if (stop && *stop)
        {
            ok = 0;
            break;
        }

        size_t chunk = len < COPY_BUFFER_SIZE ? (size_t)len : COPY_BUFFER_SIZE;
        ssize_t n = pread(in_fd, buf, chunk, (off_t)offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
        {
            fprintf(stderr, "\n[!] Lesefehler bei Byte %" PRIu64 ": %s\n", offset,
                    n == 0 ? "Datei zu kurz" : strerror(errno));
            ok = 0;
            break;
        }

        if (is_zero(buf, (size_t)n))
        {
            if (lseek(out_fd, n, SEEK_CUR) < 0)
            {
                ok = 0;
                break;
            }
        }
        else if (!write_all(out_fd, buf, (size_t)n))
        {
            fprintf(stderr, "\n[!] Schreibfehler: %s\n", strerror(errno));
            ok = 0;
            break;
        }

        offset += (uint64_t)n;
        len -= (uint64_t)n;
    }

    free(buf);
    return ok;
}

int merge_image(const char *raw_image, const char *decrypted_file, const PartitionInfo *info,
                const char *merged_path, const volatile sig_atomic_t *stop)
{
    int raw_fd = -1, dec_fd = -1, out_fd = -1, ok = 0;
    uint64_t raw_size, dec_size;
    uint64_t part_offset = info->start * info->sector_size;
    uint64_t part_bytes = info->length * info->sector_size;

    raw_fd = open(raw_image, O_RDONLY);
    dec_fd = open(decrypted_file, O_RDONLY);
    if (raw_fd < 0 || dec_fd < 0)
    {
        perror("[!] Image konnte nicht geöffnet werden");
        goto out;
    }
    if (!get_fd_size(raw_fd, &raw_size) || !get_fd_size(dec_fd, &dec_size))
    {
        fprintf(stderr, "[!] Imagegröße konnte nicht ermittelt werden.\n");
        goto out;
    }
    if (part_offset + part_bytes > raw_size)
    {
        fprintf(stderr, "[!] Partition reicht über das Ende des Images hinaus.\n");
        goto out;
    }
    if (dec_size != part_bytes)
    {
        fprintf(stderr, "[!] Entschlüsselte Partition hat %" PRIu64 " Byte, erwartet wurden %" PRIu64 " Byte.\n",
                dec_size, part_bytes);
        goto out;
    }

    out_fd = open(merged_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (out_fd < 0)
    {
        if (errno == EEXIST)
            fprintf(stderr, "[!] %s existiert bereits und wird nicht überschrieben.\n", merged_path);
        else
            perror("[!] Zieldatei konnte nicht angelegt werden");
        goto out;
    }

    uint64_t tail_offset = part_offset + part_bytes;
    printf("[*] Kopiere Bereich vor der Partition (%" PRIu64 " Byte)...\n", part_offset);
    if (!copy_range(raw_fd, 0, part_offset, out_fd, stop))
        goto out;
    printf("[*] Kopiere entschlüsselte Partition (%" PRIu64 " Byte)...\n", part_bytes);
    if (!copy_range(dec_fd, 0, part_bytes, out_fd, stop))
        goto out;
    printf("[*] Kopiere Bereich nach der Partition (%" PRIu64 " Byte)...\n", raw_size - tail_offset);
    if (!copy_range(raw_fd, tail_offset, raw_size - tail_offset, out_fd, stop))
        goto out;

    // Übersprungene Nullblöcke am Ende brauchen die richtige Dateigröße
    if (ftruncate(out_fd, (off_t)raw_size) != 0 || fsync(out_fd) != 0)
    {
        perror("[!] Zieldatei konnte nicht abgeschlossen werden");
        goto out;
    }
    ok = 1;

out:
    if (raw_fd >= 0)
        close(raw_fd);
    if (dec_fd >= 0)
        close(dec_fd);
    if (out_fd >= 0)
    {
        close(out_fd);
        if (!ok)
            unlink(merged_path);
    }
    return ok;
}
