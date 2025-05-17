#ifndef DISLOCKER_RUNNER_H
#define DISLOCKER_RUNNER_H

#include <stdint.h>

/**
 * Führt das Programm dislocker aus, um ein verschlüsseltes BitLocker-Image zu entschlüsseln.
 *
 * @param image_path   Pfad zur verschlüsselten Image-Datei.
 * @param start_sector Startsektor im Image (Offset in Sektoren).
 * @param key          Entschlüsselungs-Schlüssel.
 * @param output_path  Pfad, unter dem das entschlüsselte Volume abgelegt wird.
 * @return             0 bei Erfolg, ungleich 0 im Fehlerfall.
 */
int run_dislocker(const char *image_path, uint64_t start_sector, const char *key, const char *output_path);

#endif
