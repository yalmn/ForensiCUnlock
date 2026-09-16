// include/dislocker_runner.h
#ifndef DISLOCKER_RUNNER_H
#define DISLOCKER_RUNNER_H

#include <stdint.h>

/**
 * Entschlüsselt ein BitLocker-Volume mit dislocker (read-only). Das Ergebnis liegt
 * danach als Datei output_dir/dislocker-file vor. output_dir ist ein FUSE-Mount und
 * muss später mit unmount_fuse wieder ausgehängt werden.
 *
 * @param image_path    Pfad zum Image oder Blockgerät.
 * @param offset_bytes  Beginn des BitLocker-Volumes im Image in Byte.
 * @param key           BitLocker-Wiederherstellungsschlüssel (48 Ziffern).
 * @param output_dir    Verzeichnis, in das dislocker einhängt.
 * @return              1 bei Erfolg, 0 bei Fehler.
 */
int run_dislocker(const char *image_path, uint64_t offset_bytes, const char *key, const char *output_dir);

#endif
