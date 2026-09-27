// include/eow.h
#ifndef EOW_H
#define EOW_H

#include <stddef.h>
#include <stdint.h>

/*
 * BitLocker Encrypt-on-Write (EOW), die Betriebsart „nur belegten Speicher
 * verschlüsseln“ neuerer Windows-Versionen. Ein Teil des Volumes bleibt dabei im
 * Klartext. Welche Blöcke verschlüsselt sind, steht in einer Bitmap in den
 * BitLocker-Metadaten. Wer das ganze Volume entschlüsselt, macht die
 * Klartextblöcke unbrauchbar.
 *
 * Aufbau nach der libbde-Dokumentation (libyal, „BitLocker Drive Encryption (BDE)
 * format“, Abschnitte EOW descriptor, block map, block record). Die Prüfsummen sind
 * CRC32 (IEEE), gegen ein echtes Windows-11-Volume geprüft.
 */

/** Art eines Bereichs im entschlüsselten Volume. */
typedef enum
{
    EOW_PLAIN = 1, // im Original unverschlüsselt: Bytes unverändert aus dem Original
    EOW_META = 2   // EOW-Verwaltungsdaten: wie libbde mit Nullen ausgeben
} EowKind;

/** Ein Bereich relativ zum Volumebeginn. Alles außerhalb gilt als verschlüsselt. */
typedef struct
{
    uint64_t offset;
    uint64_t length;
    EowKind kind;
} EowRange;

/** Angaben zu einer Block-Map für das Protokoll. */
typedef struct
{
    uint32_t index;
    uint64_t block_map_offset;
    uint64_t region_offset;
    uint64_t region_size;
    uint64_t sequence;
    uint32_t bits;
    uint32_t bits_set;
} EowMapInfo;

/** Ausgewertete EOW-Informationen eines Volumes. */
typedef struct
{
    uint64_t descriptor_offset;
    uint32_t relocation_block_size;
    EowMapInfo *maps;
    size_t map_count;
    EowRange *ranges; // sortiert, überlappungsfrei, nur EOW_PLAIN und EOW_META
    size_t range_count;
    uint64_t plain_bytes;
    uint64_t meta_bytes;
} EowMap;

/**
 * Liest die EOW-Informationen eines BitLocker-Volumes, das bei part_offset im
 * Image beginnt. Alle Strukturen werden über ihre CRC32 geprüft.
 *
 * @return  1 wenn EOW vorhanden und vollständig gelesen, 0 wenn das Volume kein
 *          EOW nutzt, -1 wenn EOW vorhanden, aber nicht verlässlich lesbar ist
 *          (err enthält dann den Grund).
 */
int eow_read(int fd, uint64_t part_offset, uint64_t part_bytes, EowMap *map, char *err, size_t err_len);

/** Gibt den Speicher einer EowMap frei. */
void eow_free(EowMap *map);

/**
 * Schreibt ein Protokoll aller Entscheidungen (Block-Maps, Bereiche, Summen).
 *
 * @return  1 bei Erfolg, 0 bei Fehler.
 */
int eow_write_report(const EowMap *map, const char *path);

/** CRC32 (IEEE 802.3, wie zlib). */
uint32_t eow_crc32(const unsigned char *data, size_t len);

#endif
