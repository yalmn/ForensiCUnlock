// partition_parser.h
#ifndef PARTITION_PARSER_H
#define PARTITION_PARSER_H

#include <stdint.h>

/**
 * Struktur zur Beschreibung einer Partition innerhalb eines Images.
 *
 * @param slot    Index oder Nummer der Partition (z. B. Partitionstabelle-Eintrag).
 * @param start   Startsektor der Partition innerhalb des Images (in Sektoren).
 * @param length  Länge der Partition (Anzahl der Sektoren).
 */
typedef struct
{
    int slot;
    uint64_t start;
    uint64_t length;
} PartitionInfo;

/**
 * Durchsucht das angegebene Image nach der BitLocker-Datenpartition (BDP).
 *
 * @param image_path  Pfad zur Image-Datei, in der die Partition gesucht wird.
 * @param info        Zeiger auf eine PartitionInfo-Struktur, in der die gefundenen
 *                    Partitionseigenschaften (slot, start, length) abgelegt werden.
 * @return            0, wenn die BDP-Partition erfolgreich gefunden und gefüllt wurde;
 *                    ungleich 0, wenn keine geeignete Partition gefunden wurde oder ein Fehler auftrat.
 */
int find_bdp_partition(const char *image_path, PartitionInfo *info);

#endif
