// include/ova_mounter.h
#ifndef OVA_MOUNTER_H
#define OVA_MOUNTER_H

#include <limits.h>
#include <stddef.h>

#define OVA_MAX_DISKS 16

/**
 * Beschreibt eine aus einer OVA extrahierte VMDK-Disk.
 *
 * vmdk_path  Pfad zur extrahierten VMDK im Arbeitsverzeichnis.
 * label      Anzeigename (Dateiname der VMDK innerhalb der OVA).
 */
typedef struct
{
    char vmdk_path[PATH_MAX];
    char label[160];
} OvaDisk;

/**
 * Prüft anhand der Dateiendung, ob es sich um eine OVA-Datei handelt
 * (.ova, Groß- und Kleinschreibung egal).
 *
 * @return  1 bei OVA-Endung, sonst 0.
 */
int is_ova_path(const char *path);

/**
 * Extrahiert alle VMDK-Disks aus einer OVA (tar-Archiv) in work_dir.
 *
 * @param ova_path   Pfad zur OVA-Datei.
 * @param work_dir   Verzeichnis, in das die VMDKs extrahiert werden (wird angelegt).
 * @param list       Ausgabe: gefundene und extrahierte Disks.
 * @param max        Größe von list (höchstens OVA_MAX_DISKS).
 * @return           Anzahl extrahierter Disks, -1 bei Fehler.
 */
int extract_ova_disks(const char *ova_path, const char *work_dir, OvaDisk *list, int max);

/**
 * Hängt eine VMDK read-only per qemu-nbd als Blockgerät ein. Lädt bei Bedarf
 * das nbd-Kernelmodul und sucht ein freies /dev/nbdX. Das Gerät muss später
 * mit nbd_disconnect wieder gelöst werden.
 *
 * @param vmdk_path   Pfad zur (extrahierten) VMDK.
 * @param device_out  Ausgabe: Pfad des belegten Blockgeräts (z. B. /dev/nbd0).
 * @param len         Größe des Puffers device_out.
 * @return            1 bei Erfolg, 0 bei Fehler.
 */
int nbd_connect(const char *vmdk_path, char *device_out, size_t len);

/**
 * Löst ein zuvor mit nbd_connect verbundenes Blockgerät wieder (qemu-nbd -d).
 *
 * @param device  Gerätepfad (z. B. /dev/nbd0).
 * @return        1 bei Erfolg, sonst 0.
 */
int nbd_disconnect(const char *device);

#endif
