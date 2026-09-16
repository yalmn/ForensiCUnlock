// include/image_converter.h
#ifndef IMAGE_CONVERTER_H
#define IMAGE_CONVERTER_H

#include <stddef.h>

#define EWF_MAX_SEGMENTS 4096

/**
 * Prüft anhand der Dateiendung, ob es sich um ein EWF-Image handelt
 * (.E01 bis .EZZ, .Ex01 oder .ewf, Groß- und Kleinschreibung egal).
 *
 * @return  1 bei EWF-Endung, sonst 0.
 */
int is_ewf_path(const char *path);

/**
 * Liefert die Dateiendung des n-ten EWF-Segments passend zur Endung des ersten Segments.
 * Beispiel für "E01": 1 -> E01, 99 -> E99, 100 -> EAA, 101 -> EAB.
 *
 * @param first_ext  Endung des ersten Segments ohne Punkt ("E01", "e01", "Ex01").
 * @param n          Segmentnummer ab 1.
 * @param out        Puffer für die Endung ohne Punkt.
 * @param len        Größe des Puffers.
 * @return           1 bei Erfolg, 0 wenn n außerhalb des gültigen Bereichs liegt.
 */
int ewf_segment_ext(const char *first_ext, int n, char *out, size_t len);

/**
 * Prüft die Segmentdateien eines EWF-Images. Der Pfad muss auf das erste Segment
 * zeigen. Alle Segmente müssen lückenlos im selben Verzeichnis liegen.
 *
 * @param first_segment  Pfad zur ersten Segmentdatei (z. B. /cases/disk.E01).
 * @param count          Ausgabe: Anzahl der gefundenen Segmente.
 * @return               1 wenn die Segmente vollständig sind, sonst 0.
 */
int check_ewf_segments(const char *first_segment, int *count);

/**
 * Stellt ein EWF-Image mit ewfmount als RAW-Datei bereit. libewf liest dabei
 * automatisch alle Segmente (E01, E02, ...) aus dem Verzeichnis des ersten Segments.
 *
 * @param first_segment  Pfad zur ersten Segmentdatei.
 * @param mount_dir      Verzeichnis, in das eingehängt wird.
 * @param raw_path       Ausgabe: Pfad zur RAW-Sicht (mount_dir/ewf1).
 * @param len            Größe des Puffers raw_path.
 * @return               1 bei Erfolg, 0 bei Fehler.
 */
int mount_ewf(const char *first_segment, const char *mount_dir, char *raw_path, size_t len);

#endif
