// include/image_merger.h
#ifndef IMAGE_MERGER_H
#define IMAGE_MERGER_H

#include <stdint.h>
#include "partition_parser.h"  // Für PartitionInfo-Struktur

/**
 * Führt das Aufteilen des Images in drei Teile durch, ersetzt den BDP-Bereich mit der
 * entschlüsselten Partition, merged das Ergebnis in eine Datei `merged.dd` und löscht
 * alle temporären Dateien.
 *
 * @param raw_image        Pfad zum Originalimage (RAW oder aus EWF konvertiert).
 * @param dislocker_file   Pfad zur entschlüsselten Partition (z. B. dislocker-file).
 * @param bdp_info         Informationen zur BDP-Partition (Startsektor, Länge).
 * @param output_dir       Arbeitsverzeichnis für alle temporären und finalen Dateien.
 * @return                 1 bei Erfolg, 0 bei Fehler.
 */
int merge_and_cleanup(const char *raw_image, const char *dislocker_file, const PartitionInfo *bdp_info, const char *output_dir);

#endif // IMAGE_MERGER_H
