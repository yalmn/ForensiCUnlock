// include/dislocker_runner.h
#ifndef DISLOCKER_RUNNER_H
#define DISLOCKER_RUNNER_H

#include <stdint.h>

/**
 * Art des Entsperr-Schlüssels, den dislocker verwenden soll.
 */
typedef enum
{
    KEY_RECOVERY, /**< 48-stelliger Wiederherstellungsschlüssel (dislocker -p). */
    KEY_VMK       /**< Datei mit dem Volume Master Key (dislocker -K). */
} KeyMode;

/**
 * Baut die dislocker-Schlüsselargumente für den gewählten Modus.
 *
 * KEY_RECOVERY: legt ein einzelnes Argument "-p<key>" per malloc in *heap_arg ab
 * und lässt out[0] darauf zeigen (der Aufrufer gibt *heap_arg frei). KEY_VMK:
 * setzt out[0] = "-K" und out[1] = key (Dateipfad), *heap_arg bleibt NULL.
 *
 * @param mode      Schlüsselart.
 * @param key       Recovery-Key bzw. Pfad zur VMK-Datei.
 * @param out       Ziel für die Argumentzeiger (Platz für 2).
 * @param heap_arg  Nimmt bei KEY_RECOVERY den allokierten Puffer auf, sonst NULL.
 * @return          Anzahl gesetzter Argumente (1 oder 2), 0 bei Fehler.
 */
int build_key_args(KeyMode mode, const char *key, const char *out[2], char **heap_arg);

/**
 * Entschlüsselt ein BitLocker-Volume mit dislocker (read-only). Das Ergebnis liegt
 * danach als Datei output_dir/dislocker-file vor. output_dir ist ein FUSE-Mount und
 * muss später mit unmount_fuse wieder ausgehängt werden.
 *
 * @param image_path    Pfad zum Image oder Blockgerät.
 * @param offset_bytes  Beginn des BitLocker-Volumes im Image in Byte.
 * @param mode          Schlüsselart (Recovery-Key oder VMK-Datei).
 * @param key           Wiederherstellungsschlüssel (48 Ziffern) bzw. VMK-Dateipfad.
 * @param output_dir    Verzeichnis, in das dislocker einhängt.
 * @return              1 bei Erfolg, 0 bei Fehler.
 */
int run_dislocker(const char *image_path, uint64_t offset_bytes, KeyMode mode, const char *key, const char *output_dir);

/**
 * Schreibt die BitLocker-Metadaten des Volumes mit dislocker-metadata als Text
 * nach out_file. Metadaten sind unverschlüsselt, es wird kein Schlüssel benötigt.
 * Enthält Verschlüsselungsmethode, Volume-GUID, Protektoren und Offsets.
 *
 * @param image_path    Pfad zum Image oder Blockgerät.
 * @param offset_bytes  Beginn des BitLocker-Volumes im Image in Byte.
 * @param out_file      Zieldatei für die Textausgabe (z. B. metadata.txt).
 * @return              1 bei Erfolg, 0 bei Fehler.
 */
int dump_dislocker_metadata(const char *image_path, uint64_t offset_bytes, const char *out_file);

#endif
