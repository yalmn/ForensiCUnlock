// include/loop_device.h
#ifndef LOOP_DEVICE_H
#define LOOP_DEVICE_H

/**
 * Legt ein Loop-Device für die angegebene Datei an und gibt den zugewiesenen
 * Loop-Gerätepfad zurück.
 *
 * @param file_path  Pfad zur Datei, die als Blockgerät verwendet werden soll.
 * @param loop_path  Puffer, in dem der Pfad zum erstellten Loop-Device abgelegt wird.
 * @param max_len    Maximale Länge des Puffers `loop_path`.
 * @return           0 bei Erfolg, ungleich 0 im Fehlerfall.
 */
int setup_loop_device(const char *file_path, char *loop_path, size_t max_len);

#endif