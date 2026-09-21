// include/partition_parser.h
#ifndef PARTITION_PARSER_H
#define PARTITION_PARSER_H

#include <stdint.h>

#define MAX_PARTITIONS 128

/**
 * Beschreibung einer Partition innerhalb eines Images.
 *
 * slot         Eintrag in der mmls-Ausgabe (z. B. 5 für "005:").
 * start        Startsektor der Partition.
 * length       Länge der Partition in Sektoren.
 * sector_size  Sektorgröße in Byte laut mmls (512 oder 4096).
 * description  Beschreibung aus mmls (bei GPT der Partitionsname, kann leer sein).
 */
typedef struct
{
    int slot;
    uint64_t start;
    uint64_t length;
    uint32_t sector_size;
    char description[128];
} PartitionInfo;

/**
 * Liest die Sektorgröße aus der mmls-Zeile "Units are in N-byte sectors".
 *
 * @return  1 wenn die Zeile passt und sector_size gesetzt wurde, sonst 0.
 */
int parse_mmls_units(const char *line, uint32_t *sector_size);

/**
 * Liest eine Partitionszeile aus der mmls-Ausgabe. Meta-Einträge und
 * nicht zugeordnete Bereiche werden übersprungen.
 *
 * @return  1 wenn die Zeile eine echte Partition beschreibt, sonst 0.
 */
int parse_mmls_entry(const char *line, PartitionInfo *info);

/**
 * Prüft, ob an der angegebenen Byte-Position eines Images ein BitLocker-Volume beginnt.
 * Erkannt werden die Signatur "-FVE-FS-" (ab Windows 7) und die BitLocker-GUID
 * bei BitLocker To Go bzw. Vista.
 *
 * @return  1 bei BitLocker, sonst 0.
 */
int has_bitlocker_signature(const char *image_path, uint64_t offset);

/**
 * Sucht mit mmls alle BitLocker-Partitionen im Image.
 *
 * @param image_path  Pfad zum RAW-Image oder Blockgerät.
 * @param list        Ausgabe: gefundene Partitionen.
 * @param max         Größe von list.
 * @return            Anzahl gefundener BitLocker-Partitionen, -1 wenn mmls fehlschlägt.
 */
int find_bitlocker_partitions(const char *image_path, PartitionInfo *list, int max);

/**
 * Schreibt die Daten der gewählten Partition als Textdatei (bdp.info).
 *
 * @return  1 bei Erfolg, 0 bei Fehler.
 */
int write_partition_info(const char *path, const PartitionInfo *info);

#endif
