# ForensiCUnlock

**ForensiCUnlock** ist ein spezialisiertes C-basiertes Tool zur automatisierten Entschlüsselung von BitLocker-Partitionen in forensischen Images. Es analysiert das Beweismittel, identifiziert automatisch die Basic Data Partition (BDP), entschlüsselt sie mit dislocker, richtet ein Device-Mapper-Device ein und stellt das entschlüsselte, gemergte Image als Blockgerät zur Verfügung – alles vollständig nachvollziehbar und modular.

---

## 🔍 Funktionalität

ForensiCUnlock automatisiert folgende forensische Entschlüsselungs- und Analyseprozesse:

- Konvertierung von `.E01`/`.ewf` → `.dd` (RAW) via `xmount`
- Analyse der Partitionstabelle via `mmls` oder `parted` und Extraktion der BitLocker-BDP
- Entschlüsselung der BitLocker-Partition mittels `dislocker`
- Setup von Loop-Devices (original + entschlüsselt)
- Erstellung eines Mappings in `dmsetup.txt`
- Erzeugung eines virtuellen gemergten Devices `/dev/mapper/merged`
- Alle Daten in einem analysenspezifischen Arbeitsordner (z. B. `/mnt/output/run_xyz`)

---

## 🧩 Modulübersicht

| Modul              | Funktion                                                                          |
| ------------------ | --------------------------------------------------------------------------------- |
| `mount_selector`   | Mountet Beweismittel und unterscheidet EWF-Container, Roh-Image oder Block-Device |
| `image_converter`  | Führt `xmount` zur Konvertierung von EWF → RAW durch                              |
| `partition_parser` | Analysiert das Image mit `parted` oder `mmls` und erkennt automatisch die BDP     |
| `dislocker_runner` | Führt Entschlüsselung via `dislocker` mit Start-Offset durch                      |
| `loop_device`      | Erstellt `loop`-Geräte aus den relevanten Image-Dateien                           |
| `mapper`           | Erstellt Mapping-Datei (`dmsetup.txt`) und setzt `/dev/mapper/merged`             |
| `main.c`           | Zentrale Steuerung mit Übergabe aller Argumente                                   |

---

## ⚙️ Installation

### 1. Lokale Installation unter Debian-basierten Systemen

```bash
git clone https://github.com/dein-benutzer/ForensiCUnlock.git
cd ForensiCUnlock
chmod +x scripts/install.sh
./scripts/install.sh
```

### 2. Docker-basierte Ausführung

```bash
docker build -t forensicunlock .
```

Beispielausführung im Container:

```bash
./scripts/run-docker.sh /dev/sdb2 "BITLOCKER-KEY" /mnt/output/run_case01
```

---

## 🛠️ Nutzung

### 🔐 Beispiel (lokal):

```bash
sudo ./forensic_unlock /dev/sdb2 "BITLOCKER-KEY" /mnt/output/run_case01
```

Das Tool erstellt folgende Struktur:

```text
/mnt/output/run_case01/
├── bitlocker/
│   └── dislocker-file      # entschlüsselter Block-Container
├── xmount/
│   └── image.dd            # konvertiertes Roh-Image
├── dmsetup.txt             # Device-Mapper-Konfiguration
```

Nach Abschluss existiert das virtuelle, entschlüsselte Blockgerät:

```bash
ls -l /dev/mapper/merged
# /dev/mapper/merged → Zugriff auf das vollständige, entschlüsselte Image
```

Falls du das Image zur weiteren Analyse als Datei sichern möchtest:

```bash
sudo dd if=/dev/mapper/merged of=/mnt/output/run_case01/merged_image.dd bs=1M status=progress
```

---

## 📋 Ausgabeordnerstruktur

Alle relevanten Daten befinden sich im `output_folder`, den du selbst als Argument angibst:

```text
/mnt/output/run_case01/
├── bitlocker/
│   └── dislocker-file
├── xmount/
│   └── image.dd
└── dmsetup.txt
```

---

## 📋 Beispiel-Szenario: Analyse eines EWF-Beweismittels

1. Du erhältst ein `.E01`-Image auf einem Datenträger oder als Datei.
2. Starte das Tool:
   ```bash
   sudo ./forensic_unlock /path/to/image.E01 MYKEY123 /mnt/output/run_case01
   ```
3. Ablauf:
   - Mount von Beweismittel (ewfmount oder Loop-Mount)
   - `.E01` → `.dd`-Konvertierung
   - Partitionserkennung (Start & Länge der BDP)
   - BitLocker-Entschlüsselung
   - Mapping & Merge
4. Ergebnis:
   - Virtuelles Device `/dev/mapper/merged` mit vollständigem, entschlüsseltem Image
   - Bei Bedarf Datei-Export via `dd if=/dev/mapper/merged of=...`

---

## 🔎 Hinweise

- Nur unter **Linux** lauffähig (getestet unter Debian/Kali).
- Root-/`sudo`-Rechte erforderlich.
- Alle temporären Daten landen im gewählten `output_folder` (pro Fall isoliert).
- Kein separater Cleanup nötig – Arbeitsordner ist bereits trennscharf.

---

## 👨‍💻 Autor & Lizenz

Open Source & frei verwendbar unter MIT-Lizenz.

> Dieses Projekt wurde entwickelt für forensisch wiederholbare und automatisierte BDP-Extraktion aus verschlüsselten Windows-Systemen.
