# ForensiCUnlock

ForensiCUnlock ist ein forensisches Entschlüsselungs-Tool, das speziell für die Verarbeitung von BitLocker-verschlüsselten Partitionen in EWF- oder RAW-Images konzipiert wurde. Es automatisiert die komplette Analysepipeline – von der Image-Erkennung, Konvertierung, Analyse bis zur Entschlüsselung und logischen Zusammenführung über dmsetup.

---

## Funktionsweise

ForensiCUnlock besteht aus mehreren Modulen, die jeweils für eine bestimmte Stufe der Entschlüsselung und Analyse zuständig sind.

### main.c

Zentrale Steuereinheit. Ruft alle Teilmodule auf, prüft Rechte und erwartet:

- Gerät oder Partition (z. B. /dev/sdb1)
- BitLocker-Schlüssel
- Zielordner für alle Ausgaben

### mount_selector.c

Mountet das Beweismittel (z. B. /dev/sdb1) und sucht nach .E01- oder .ewf-Dateien. Gibt das passende Verzeichnis zurück.

### image_converter.c

Wandelt das EWF-Image mithilfe von xmount in ein RAW-Image (.dd) um. Die RAW-Datei wird im Unterordner `xmount/` im Arbeitsverzeichnis abgelegt.

### partition_parser.c

Führt mmls aus, extrahiert automatisch die Partition mit der Bezeichnung "Basic data partition" (BDP). Erkennt deren Start-Offset und Länge. Die mmls-Ausgabe wird angezeigt, bevor fortgefahren wird.

### dislocker_runner.c

Startet dislocker mit dem berechneten Offset auf der RAW-Datei. Der BitLocker-Schlüssel wird übergeben. Die entschlüsselte Datei `dislocker-file` wird im Unterordner `bitlocker/` gespeichert.

### loop_device.c

Bindet das Originalimage und die entschlüsselte Datei als Loop-Devices ein. Gibt die Pfade der zugehörigen /dev/loopX-Geräte zurück.

### mapper.c

Erstellt die Mapping-Datei `dmsetup.txt`, die folgende Zonen beschreibt:

- Vor der verschlüsselten Partition → aus originalem Image
- Entschlüsselter Bereich → aus dislocker-file
- Nachfolgende Sektoren → wieder aus originalem Image

Setzt anschließend das zusammengeführte Gerät unter `/dev/mapper/merged` mit Hilfe von `dmsetup create`.

---

## Installation

### Lokale Ausführung (Linux)

1. Abhängigkeiten installieren (automatisiert):

```bash
sudo ./scripts/install.sh
```

2. Kompilieren:

```bash
make
```

3. Starten:

```bash
sudo ./forensic_unlock <device_or_image> <bitlocker_key> <output_folder>
```

### Docker-Ausführung

1. Docker-Image erstellen:

```bash
docker build -t forensicunlock .
```

2. Ausführen:

```bash
sudo docker run --rm -it --privileged \
    -v /dev:/dev \
    -v /mnt/output:/mnt/output \
    forensicunlock /dev/sdb1 "MEIN-SCHLUESSEL" /mnt/output/Fall_ABC
```

### Alternativ: Ausführung über run-docker.sh

Erstelle oder nutze das Skript `run-docker.sh`, um Docker einfach mit Parametern zu starten:

```bash
#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "[!] Bitte als root oder mit sudo ausführen."
  exit 1
fi

if [ "$#" -ne 3 ]; then
  echo "Usage: sudo ./run-docker.sh <device_or_image> <bitlocker_key> <output_path>"
  echo "Beispiel: sudo ./run-docker.sh /dev/sdb1 ABCD-1234 ~/Desktop/FallXYZ"
  exit 1
fi

DEVICE=$1
KEY=$2
OUT_PATH=$3

ABS_OUT_PATH=$(realpath "$OUT_PATH")
mkdir -p "$ABS_OUT_PATH"

docker run --rm -it --privileged \
  -v "$DEVICE":"$DEVICE" \
  -v "$ABS_OUT_PATH":"$ABS_OUT_PATH" \
  forensicunlock "$DEVICE" "$KEY" "$ABS_OUT_PATH"
```

Dann einfach aufrufen mit:

```bash
sudo ./run-docker.sh /dev/sdb1 "BITLOCKER-KEY" ~/Desktop/FallXYZ
```

---

## Nutzung

### Syntax

```bash
sudo ./forensic_unlock <device_or_image> <bitlocker_key> <output_folder>
```

### Beispiel

```bash
sudo ./forensic_unlock /dev/sdb1 "ABC1-DEF2-KEY3" ~/Desktop/Fall_XYZ
```

---

## Ablauf

1. `/dev/sdb1` wird nach `/mnt/output` gemountet
2. EWF-Verzeichnis wird erkannt
3. `.E01` → `.dd` Konvertierung unter `output_folder/xmount/image.dd`
4. `mmls` zeigt Partitionstabelle, Benutzer bestätigt per ENTER
5. BDP wird analysiert, dislocker entschlüsselt diesen Bereich
6. dislocker-file unter `output_folder/bitlocker/dislocker-file`
7. Zwei Loop-Devices werden erstellt
8. Mapping-Datei `output_folder/dmsetup.txt` wird generiert
9. `dmsetup` erzeugt `/dev/mapper/merged` für Mounting/Analyse

---

## Ausgabestruktur

Der gesamte Output wird unter dem vom Benutzer angegebenen Zielordner gespeichert.

Beispiel: `~/Desktop/Fall_XYZ`

```
~/Desktop/Fall_XYZ/
├── xmount/
│   └── image.dd
├── bitlocker/
│   └── dislocker-file
├── dmsetup.txt
```

---

## Ergebnisbereitstellung

Nach erfolgreichem Lauf:

- Loop Device (Original): /dev/loopX
- Loop Device (Entschlüsselt): /dev/loopY
- Zusammengeführtes Gerät: /dev/mapper/merged

---

## Optional: Manuelles Mounten

```bash
sudo mkdir /mnt/final
sudo mount /dev/mapper/merged /mnt/final
```

---

## Hinweise

- Das Tool benötigt zwingend Root-Rechte
- Alle Ausgaben liegen im Zielordner – es ist kein separater Cleanup nötig
- Die `dmsetup.txt` wird dynamisch auf Basis der Partitionsstruktur erzeugt

---
