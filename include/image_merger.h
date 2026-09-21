// include/image_merger.h
#ifndef IMAGE_MERGER_H
#define IMAGE_MERGER_H

#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include "partition_parser.h"

/**
 * Ermittelt die Größe eines Images in Byte. Funktioniert für normale Dateien und Blockgeräte.
 *
 * @return  1 bei Erfolg, 0 bei Fehler.
 */
int get_image_size(const char *path, uint64_t *size);

/**
 * Kopiert len Byte ab offset aus in_fd an die aktuelle Position von out_fd.
 * Blöcke, die nur aus Nullen bestehen, werden übersprungen (die Ausgabe wird sparse).
 *
 * @param stop  Wird während des Kopierens geprüft. Ist der Wert ungleich 0, bricht die Funktion ab.
 * @return      1 bei Erfolg, 0 bei Lese-/Schreibfehler oder Abbruch.
 */
int copy_range(int in_fd, uint64_t offset, uint64_t len, int out_fd, const volatile sig_atomic_t *stop);

/**
 * Schreibt das vollständige entschlüsselte Image: Bereich vor der Partition aus dem
 * Original, danach die entschlüsselte Partition, danach der Rest aus dem Original.
 * Eine bestehende Zieldatei wird nie überschrieben. Bei Fehler wird die Zieldatei gelöscht.
 *
 * @param raw_image       Original-Image (RAW, ewf1 oder Blockgerät).
 * @param decrypted_file  Entschlüsselte Partition (dislocker-file).
 * @param info            Lage der Partition im Original.
 * @param merged_path     Pfad der neuen Image-Datei (merged.dd).
 * @param stop            Abbruch-Flag (z. B. durch Ctrl+C gesetzt), darf NULL sein.
 * @return                1 bei Erfolg, 0 bei Fehler.
 */
int merge_image(const char *raw_image, const char *decrypted_file, const PartitionInfo *info,
                const char *merged_path, const volatile sig_atomic_t *stop);

#endif
