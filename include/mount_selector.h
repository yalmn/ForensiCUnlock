// include/mount_selector.h
#ifndef MOUNT_SELECTOR_H
#define MOUNT_SELECTOR_H

#include <stddef.h>

/**
 * Bindet automatisch das angegebene Device ein und durchsucht das Einhängeverzeichnis
 * nach einem EWF-Image (z. B. *.E01). Gibt das gefundene Verzeichnis oder den Pfad zur
 * ersten Segmentdatei zurück.
 *
 * @param device        Pfad zum Blockdevice, das eingehängt werden soll (z. B. "/dev/loop0").
 * @param selected_dir  Puffer, in dem der Pfad zum gefundenen EWF-Verzeichnis oder zur ersten
 *                      gefundenen EWF-Segmentdatei abgelegt wird.
 * @param max_len       Maximale Länge des Puffers `selected_dir`.
 * @return              0 bei Erfolg (EWF-Image gefunden), ungleich 0 im Fehlerfall
 *                      (z. B. Einhängen fehlgeschlagen oder keine EWF-Dateien gefunden).
 */
int auto_mount_and_find_ewf(const char *device, char *selected_dir, size_t max_len);

#endif
