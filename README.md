# ForensiCUnlock

**ForensiCUnlock** ist ein spezialisiertes C-basiertes Tool zur automatisierten Entschlüsselung von BitLocker-Partitionen in forensischen Images. Es analysiert das Beweismittel, identifiziert automatisch die Basic Data Partition (BDP), entschlüsselt sie mit dislocker, führt ein Device-Mapping durch und bereitet das Image für weitere Analyse vor – alles vollständig nachvollziehbar und modular.

---

## 🔍 Funktionalität

ForensiCUnlock automatisiert folgende forensische Entschlüsselungs- und Analyseprozesse:

- Automatisches Mounten eines physischen Geräts (z. B. `/dev/sdb1`)
- Scannen des Mountpunkts nach `.E01` / `.ewf`-Dateien
- Konvertierung von `.E01`/`.ewf` → `.dd` (RAW) via `xmount`
- Analyse der Partitionstabelle via `mmls` und Extraktion der BitLocker-BDP
- Entschlüsselung der BitLocker-Partition mittels `dislocker`
- Setup von Loop-Devices (original + entschlüsselt)
- Erstellung eines Mappings in `dmsetup.txt`
- Erzeugung eines virtuellen gemergten Devices `/dev/mapper/merged`
- Alle Daten in einem analysenspezifischen Arbeitsordner (z. B. `/mnt/output/run_xyz`)

---

## 🧩 Modulübersicht

| Modul               | Funktion                                                                 |
|---------------------|--------------------------------------------------------------------------|
| `mount_selector`    | Mountet Gerät und sucht automatisch nach `.E01` / `.ewf`-Dateien         |
| `image_converter`   | Führt `xmount` zur Konvertierung von EWF → RAW durch                     |
| `partition_parser`  | Analysiert das Image mit `mmls` und erkennt automatisch die BDP          |
| `dislocker_runner`  | Führt Entschlüsselung via `dislocker` mit Start-Offset durch             |
| `loop_device`       | Erstellt `loop`-Geräte aus den relevanten Image-Dateien                  |
| `mapper`            | Erstellt Mapping-Datei (`dmsetup.txt`) und setzt `/dev/mapper/merged`    |
| `main.c`            | Zentrale Steuerung mit Übergabe aller Argumente                          |

---

## ⚙️ Installation

### 1. Lokale Installation unter Kali Linux (oder Debian-basiert)

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

Beispielausführung:

```bash
./scripts/run-docker.sh /dev/sdb2 "BITLOCKER-KEY" /mnt/output/run_case01
```

---

## 🛠️ Nutzung

### 🔐 Beispiel (lokal):

```bash
sudo ./forensic_unlock /dev/sdb2 "BITLOCKER-KEY" /mnt/output/run_case01
```

Das Tool erstellt:

- `/mnt/output/run_case01/xmount/image.dd`
- `/mnt/output/run_case01/bitlocker/dislocker-file`
- `/mnt/output/run_case01/dmsetup.txt`
- `/dev/mapper/merged` ← virtuelles, entschlüsseltes Image zur weiteren Analyse

---

## 📦 Ausgabeordnerstruktur

Alle relevanten Daten befinden sich im `output_folder`, den du selbst als Argument angibst:

```bash
mnt/output/run_case01/
├── bitlocker/
│   └── dislocker-file
├── xmount/
│   └── image.dd
├── dmsetup.txt
```

---

## 📋 Beispiel-Szenario: Analyse eines EWF-Beweismittels

1. Du erhältst ein `.E01`-Image auf `/dev/sdb1`
2. Starte das Tool mit:

```bash
sudo ./forensic_unlock /dev/sdb1 MYKEY123 /mnt/output/run_case01
```

3. ForensiCUnlock führt automatisch durch:
   - Mount von `/dev/sdb1`
   - Konvertierung `.E01` → `.dd`
   - Partitionserkennung via `mmls`
   - Entschlüsselung
   - Mapping & Merge

4. Ergebnis: Du hast ein analysierbares, gemapptes, entschlüsseltes Image unter `/dev/mapper/merged`

---

## 🔎 Hinweise

- Nur unter **Linux** lauffähig (getestet unter Kali)
- Root-/`sudo`-Rechte erforderlich
- Alle temporären Daten landen im gewählten `output_folder` (pro Fall isoliert)
- Kein Cleanup notwendig – Arbeitsordner ist bereits trennscharf

---

## 👨‍💻 Autor & Lizenz

Open Source & frei verwendbar unter MIT-Lizenz.

> Dieses Projekt wurde entwickelt für forensisch wiederholbare und automatisierte BDP-Extraktion aus verschlüsselten Windows-Systemen.
