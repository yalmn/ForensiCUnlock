#ifndef IMAGE_CONVERTER_H
#define IMAGE_CONVERTER_H

/**
 * Konvertiert ein EWF-Image im Verzeichnis `ewf_dir` in ein RAW-Image und bindet
 * es im Verzeichnis `xmount_dir` zur weiteren Verarbeitung.
 *
 * @param ewf_dir     Pfad zum Verzeichnis mit den EWF-Segmentdateien (*.E01, *.E02, ...).
 * @param xmount_dir  Zielverzeichnis, in dem das RAW-Image mittels Xmount eingehängt wird.
 * @return            0 bei erfolgreicher Konvertierung und Einhängen, ungleich 0 bei Fehlern.
 */
int convert_ewf_to_raw(const char *ewf_dir, const char *xmount_dir);

#endif
