// include/exec_utils.h
#ifndef EXEC_UTILS_H
#define EXEC_UTILS_H

#include <stdio.h>
#include <sys/types.h>

/**
 * Startet ein externes Programm ohne Shell (fork + execvp) und wartet auf dessen Ende.
 * Argumente werden unverändert übergeben, Leerzeichen oder Anführungszeichen in Pfaden
 * und Schlüsseln sind daher unkritisch.
 *
 * @param argv  NULL-terminierte Argumentliste, argv[0] ist der Programmname.
 * @return      1 wenn das Programm mit Exit-Code 0 beendet wurde, sonst 0.
 */
int run_cmd(char *const argv[]);

/**
 * Wie run_cmd, liefert aber die Standardausgabe des Programms als lesbaren Stream.
 * Der Stream muss mit close_cmd_read geschlossen werden.
 *
 * @param argv  NULL-terminierte Argumentliste.
 * @param pid   Ausgabe: Prozess-ID des gestarteten Programms.
 * @return      Stream zum Lesen oder NULL bei Fehler.
 */
FILE *run_cmd_read(char *const argv[], pid_t *pid);

/**
 * Schließt den Stream aus run_cmd_read und wartet auf das Programmende.
 *
 * @return  1 wenn das Programm mit Exit-Code 0 beendet wurde, sonst 0.
 */
int close_cmd_read(FILE *fp, pid_t pid);

/**
 * Legt ein Verzeichnis inklusive aller fehlenden Elternverzeichnisse an (wie mkdir -p).
 *
 * @return  1 bei Erfolg oder wenn das Verzeichnis schon existiert, sonst 0.
 */
int make_dir(const char *path);

/**
 * Prüft, ob unter dem Pfad ein Dateisystem eingehängt ist.
 *
 * @return  1 wenn eingehängt, sonst 0.
 */
int is_mountpoint(const char *path);

/**
 * Hängt ein FUSE-Dateisystem (dislocker, ewfmount) aus. Versucht nacheinander
 * fusermount3, fusermount und umount und wiederholt den Versuch kurz, falls das
 * Dateisystem noch beschäftigt ist.
 *
 * @return  1 wenn danach nichts mehr eingehängt ist, sonst 0.
 */
int unmount_fuse(const char *path);

#endif
