#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>

/**
 * Erzeugt eine Mappingsdatei für Device-Mapper auf Basis eines Original- und
 * eines entschlüsselten Loop-Devices.
 *
 * @param loop_orig     Pfad zum ursprünglichen (verschlüsselten) Loop-Device.
 * @param loop_decrypted Pfad zum entschlüsselten Loop-Device.
 * @param bdp_start     Startsektor (in Sektoren) des Bereichs für die Mappingsdatei.
 * @param bdp_length    Länge (in Sektoren) des abzubildenden Bereichs.
 * @param output_path   Pfad, unter dem die erzeugte Mappingsdatei abgelegt wird.
 * @return              0 bei Erfolg, ungleich 0 im Fehlerfall.
 */
int create_mapping_file(const char *loop_orig, const char *loop_decrypted, uint64_t bdp_start, uint64_t bdp_length, const char *output_path);

/**
 * Legt ein Device-Mapper-Device auf Basis der zuvor erzeugten Mappingsdatei an.
 *
 * @param output_path   Pfad zur Mappingsdatei, die für das Device-Mapper-Device verwendet wird.
 * @return              0 bei erfolgreichem Anlegen und Aktivieren des Devices, ungleich 0 bei Fehlern.
 */
int setup_dm_device(const char *output_path);

#endif
