// src/exec_utils.c
#include "exec_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int wait_child(pid_t pid)
{
    int status;
    while (waitpid(pid, &status, 0) < 0)
    {
        // Ctrl+C unterbricht waitpid, das Kind bekommt das Signal selbst
        if (errno != EINTR)
            return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int run_cmd(char *const argv[])
{
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("[!] fork");
        return 0;
    }
    if (pid == 0)
    {
        execvp(argv[0], argv);
        fprintf(stderr, "[!] Programm '%s' konnte nicht gestartet werden: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    return wait_child(pid);
}

FILE *run_cmd_read(char *const argv[], pid_t *pid)
{
    int fds[2];
    if (pipe(fds) != 0)
    {
        perror("[!] pipe");
        return NULL;
    }

    fflush(stdout);
    fflush(stderr);

    *pid = fork();
    if (*pid < 0)
    {
        perror("[!] fork");
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (*pid == 0)
    {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execvp(argv[0], argv);
        fprintf(stderr, "[!] Programm '%s' konnte nicht gestartet werden: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    close(fds[1]);
    FILE *fp = fdopen(fds[0], "r");
    if (!fp)
    {
        close(fds[0]);
        wait_child(*pid);
    }
    return fp;
}

int close_cmd_read(FILE *fp, pid_t pid)
{
    fclose(fp);
    return wait_child(pid);
}

int make_dir(const char *path)
{
    char buf[PATH_MAX];
    if (snprintf(buf, sizeof(buf), "%s", path) >= (int)sizeof(buf))
        return 0;

    for (char *p = buf + 1; *p; p++)
    {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST)
            return 0;
        *p = '/';
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST)
        return 0;

    struct stat st;
    return stat(buf, &st) == 0 && S_ISDIR(st.st_mode);
}

int is_mountpoint(const char *path)
{
    struct stat st, parent;
    char parent_path[PATH_MAX];

    if (snprintf(parent_path, sizeof(parent_path), "%s/..", path) >= (int)sizeof(parent_path))
        return 0;
    if (stat(path, &st) != 0)
    {
        // Ein abgestürztes FUSE-Dateisystem liefert ENOTCONN, ist aber noch eingehängt
        return errno == ENOTCONN;
    }
    if (stat(parent_path, &parent) != 0)
        return 0;

    return st.st_dev != parent.st_dev || st.st_ino == parent.st_ino;
}

static int run_quiet(char *const argv[])
{
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0)
        return 0;
    if (pid == 0)
    {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    return wait_child(pid);
}

int unmount_fuse(const char *path)
{
    const struct timespec pause = {0, 200 * 1000 * 1000};

    for (int attempt = 0; attempt < 25 && is_mountpoint(path); attempt++)
    {
        char *fusermount3[] = {"fusermount3", "-u", (char *)path, NULL};
        char *fusermount[] = {"fusermount", "-u", (char *)path, NULL};
        char *umount[] = {"umount", (char *)path, NULL};

        if (run_quiet(fusermount3) || run_quiet(fusermount) || run_quiet(umount))
            continue;
        nanosleep(&pause, NULL);
    }

    if (is_mountpoint(path))
    {
        fprintf(stderr, "[!] Konnte %s nicht aushängen.\n", path);
        return 0;
    }
    return 1;
}
