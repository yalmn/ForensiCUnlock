# 🛡️ ForensiCUnlock: Automatisierte BitLocker-Entschlüsselung und Image-Zusammenführung

ForensiCUnlock ist ein vollautomatisiertes C-Tool zur Analyse und Rekonstruktion von BitLocker-verschlüsselten Partitionen aus forensischen Arbeitskopien.

## Funktionen

- Automatisches Mounten forensischer Datenträger
- Erkennung von `.E01`, `.EWF`, `.dd`, `.raw` Images
- Konvertierung von `.E01`/`.EWF` nach `.dd` mittels `xmount`
- Analyse der GPT-Partitionstabelle via `mmls` (Sleuthkit)
- Automatische Erkennung und Entschlüsselung der BDP (Basic Data Partition) mit `dislocker`
- Aufbau eines virtuellen gemergten Devices über `dmsetup`
- Erzeugung eines finalen Disk-Images mit `ddrescue`
- Automatisierter Cleanup (`cleanup.sh`)
- Docker-Integration für reproduzierbare Umgebung

---

## Voraussetzungen

Benötigte Tools (werden im Docker/Install-Skript bereitgestellt):

- `xmount`
- `dislocker`
- `ddrescue`
- `sleuthkit` (für `mmls`)
- `gcc`, `make`, `libc-dev`

---

## Deployment & Build (Linux / Kali)

```bash
git clone https://github.com/dein-user/ForensiCUnlock.git
cd ForensiCUnlock

# Setup (erstellt /mnt-Pfade, installiert Tools, kompiliert)
chmod +x scripts/install.sh
./scripts/install.sh
```

---

## Docker (alternativ)

```bash
docker build -t forensicunlock -f docker/Dockerfile .

docker run -it --privileged \
  -v /dev:/dev \
  -v /mnt:/mnt \
  forensicunlock
```

---

## Nutzung

```bash
sudo ./forensic_unlock /dev/sdX <bitlocker_recovery_key>
```

Beispiel:

```bash
sudo ./forensic_unlock /dev/sdb2 123456-789012-345678-901234-567890-123456-789012
```

Das Tool erkennt automatisch, ob es sich um eine `.E01`/`.EWF` oder `.dd`-Datei handelt.

---

## Oberflächliche Funktionsübersicht

| Modul              | Funktion                                                             |
| ------------------ | -------------------------------------------------------------------- |
| `mount_selector`   | Mountet `/dev/sdX` nach `/mnt/output`, findet Image-Verzeichnis      |
| `image_converter`  | Konvertiert `.E01`/`.EWF` mit `xmount` in `/mnt/xmount/image.dd`     |
| `partition_parser` | Analysiert Partitionstabelle mit `mmls`, erkennt BDP-Start und Länge |
| `dislocker_runner` | Entschlüsselt den BDP-Bereich basierend auf Recovery-Key             |
| `loop_device`      | Erstellt Loop-Devices aus RAW und entschlüsseltem Bereich            |
| `mapper`           | Erstellt Mapping-Datei + führt `dmsetup create` aus                  |
| `imager`           | Führt `ddrescue` durch → erzeugt `/mnt/output/merged_image.dd`       |

---

## Cleanup

```bash
sudo ./scripts/cleanup.sh
```

- Unmountet `/mnt/output`, `/mnt/xmount`, `/mnt/bitlocker`
- Löst Loop-Devices
- Entfernt `/dev/mapper/merged`

---

## Ausgabedateien

| Pfad                              | Bedeutung                          |
| --------------------------------- | ---------------------------------- |
| `/mnt/xmount/image.dd`            | Originales RAW-Image (wenn `.E01`) |
| `/mnt/bitlocker/dislocker-file`   | Entschlüsselter BDP-Bereich        |
| `/mnt/output/dmsetup.txt`         | Mapping-Datei für `dmsetup`        |
| `/dev/mapper/merged`              | Virtuelles gemergtes Device        |
| `/mnt/output/merged_image.dd`     | Finale forensische Arbeitskopie    |
| `/mnt/output/merged_image.dd.log` | Logfile von `ddrescue`             |

---

## Beispiel-Szenario

### Szenario 1: Lokale Ausführung unter Kali Linux

1. Repository klonen:

   ```bash
   git clone https://github.com/yalmn/ForensiCUnlock.git
   cd ForensiCUnlock
   ```

2. Installationsskript ausführen:

   ```bash
   chmod +x scripts/install.sh
   ./scripts/install.sh
   ```

3. Tool mit forensischem Device starten:

   ```bash
   sudo ./forensic_unlock /dev/sdb2 <bitlocker_key>
   ```

4. Entschlüsseltes, gemergtes Image unter:

   ```
   /mnt/output/merged_image.dd
   ```

5. Abschließend bereinigen:
   ```bash
   sudo ./scripts/cleanup.sh
   ```

---

### Szenario 2: Analyse in Docker-Umgebung

**Schneller Einstieg per Skript:**

```bash
chmod +x scripts/run-docker.sh
./scripts/run-docker.sh
```

Alternativ manuell:

1. Image bauen:

   ```bash
   docker build -t forensicunlock -f docker/Dockerfile .
   ```

2. Container starten mit Zugriff auf echte Devices:

   ```bash
   docker run -it --privileged \
     -v /dev:/dev \
     -v /mnt:/mnt \
     forensicunlock
   ```

3. Im Container ausführen:

   ```bash
   sudo ./forensic_unlock /dev/sdb2 <bitlocker_key>
   ```

4. Entschlüsseltes Image befindet sich auf dem gemounteten Host:

   ```
   /mnt/output/merged_image.dd
   ```

5. Nach Analyse Cleanup:

   ```bash
   sudo ./scripts/cleanup.sh
   ```

6. Ermittler erhält forensisches Image (`image.E01`) auf `/dev/sdb2`
7. Start mit:
   ```bash
   sudo ./forensic_unlock /dev/sdb2 123456-789012-...
   ```
8. Tool mountet, konvertiert, entschlüsselt und merged automatisch
9. Analysierbares Image unter `/mnt/output/merged_image.dd` verfügbar
10. Nach Analyse:
    ```bash
    sudo ./scripts/cleanup.sh
    ```
