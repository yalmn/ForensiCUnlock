[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

<p align="center">
  <img src="docs/ForensiCUnlock_Logo_watermark.png" alt="ForensiCUnlock Logo" height="300"/>
</p>

# ForensiCUnlock

**ForensiCUnlock** ist ein C-Tool für Linux und WSL2, das BitLocker-verschlüsselte Partitionen in forensischen Images entschlüsselt. Am Ende steht nicht nur die entschlüsselte Partition, sondern wieder ein vollständiges Datenträger-Image (`merged.dd`). Partitionstabelle und alle anderen Partitionen bleiben byte-genau erhalten, deshalb lässt sich das Ergebnis direkt in Autopsy, X-Ways oder The Sleuth Kit öffnen.

---

## Features

- Eingabe als RAW-Image (`.dd`, `.raw`, `.img`), EWF-Image (`.E01`, `.Ex01`, `.ewf`) oder Blockgerät (`/dev/sdX`)
- EWF-Images mit beliebig vielen Segmenten (`.E01`, `.E02`, ... `.E99`, `.EAA`, ...), Vollständigkeit wird vorab geprüft
- Erkennung der BitLocker-Partition über die Signatur im Volume-Header, unabhängig von Partitionsnamen oder -typ
- GPT und MBR, 512- und 4096-Byte-Sektoren, BitLocker To Go
- Auswahl, wenn mehrere BitLocker-Partitionen im Image liegen
- Entschlüsselung read-only mit `dislocker`, das Original wird nie verändert
- Zusammenführung direkt in eine Datei, ohne Zwischenkopien (Speicherbedarf etwa einmal die Imagegröße)
- Sauberes Aufräumen, auch bei Fehlern oder Abbruch mit Ctrl+C: alle Mounts werden ausgehängt, Hilfsordner entfernt
- Keine Shell-Aufrufe, Pfade und Schlüssel mit Leerzeichen oder Anführungszeichen sind unproblematisch

---

## Ablauf

1. **EWF bereitstellen:** Bei `.E01` prüft das Tool die Segmente und hängt das Image mit `ewfmount` als RAW-Datei ein.
2. **Partition finden:** `mmls` liest die Partitionstabelle. Jede Partition wird auf die BitLocker-Signatur (`-FVE-FS-` bzw. die BitLocker-GUID bei To Go) geprüft.
3. **Kontrolle:** Die gefundene Partition und die komplette Partitionstabelle werden angezeigt. Weiter geht es erst nach ENTER.
4. **Entschlüsseln:** `dislocker` stellt die entschlüsselte Partition read-only als `dislocker-file` bereit.
5. **Zusammenführen:** `merged.dd` entsteht aus dem Bereich vor der Partition, der entschlüsselten Partition und dem Bereich danach.
6. **Aufräumen:** `dislocker` und `ewfmount` werden ausgehängt, die Hilfsordner gelöscht.

---

## Aufbau & Module

| Modul              | Aufgabe                                                                 |
| ------------------ | ----------------------------------------------------------------------- |
| `main.c`           | Einstiegspunkt, Argumente, Ablaufsteuerung, Aufräumen und Ctrl+C        |
| `exec_utils`       | Externe Programme ohne Shell starten, Verzeichnisse anlegen, Mounts prüfen und aushängen |
| `image_converter`  | EWF-Segmente prüfen und das Image mit `ewfmount` einhängen              |
| `partition_parser` | `mmls`-Ausgabe auswerten, Sektorgröße lesen, BitLocker-Signatur prüfen  |
| `dislocker_runner` | `dislocker` mit dem passenden Byte-Offset aufrufen                      |
| `image_merger`     | Imagegröße ermitteln und `merged.dd` blockweise schreiben               |

---

## Installation

### Voraussetzungen (Debian, Ubuntu, Kali, WSL2)

```bash
sudo apt update
sudo apt install build-essential dislocker ewf-tools sleuthkit fuse3 -y
```

### Projekt klonen & kompilieren

```bash
git clone https://github.com/yalmn/ForensiCUnlock.git
cd ForensiCUnlock
make
```

Alternativ erledigt `./scripts/install.sh` beides.

---

## Verwendung

```bash
sudo ./forensic_unlock <image|device> <recovery-key> [ausgabeordner]
```

| Argument         | Bedeutung                                                              |
| ---------------- | ---------------------------------------------------------------------- |
| `image`          | RAW-Image, erstes EWF-Segment (`.E01`) oder Blockgerät                 |
| `recovery-key`   | BitLocker-Wiederherstellungsschlüssel, 48 Ziffern in 8 Blöcken         |
| `ausgabeordner`  | optional, ohne Angabe wird `./run_JJJJMMTT_HHMMSS` angelegt            |

### Beispiel mit EWF-Image

```bash
sudo ./forensic_unlock /cases/case01/disk.E01 \
    "123456-123456-123456-123456-123456-123456-123456-123456" \
    /mnt/output/case01
```

Bei EWF-Images immer das **erste** Segment angeben. Alle weiteren Segmente (`disk.E02`, `disk.E03`, ...) müssen im selben Ordner liegen und werden automatisch gefunden. Fehlt ein Segment in der Mitte, bricht das Tool mit einer Meldung ab, bevor irgendetwas eingehängt wird.

### Ergebnis

```bash
/mnt/output/case01/
├── merged.dd   # vollständiges entschlüsseltes Image
└── bdp.info    # Lage der entschlüsselten Partition (Slot, Start, Ende, Sektorgröße, Offset)
```

Die entschlüsselte Partition lässt sich danach zum Beispiel so ansehen:

```bash
mmls merged.dd
fls -o <startsektor> merged.dd
```

### Exit-Codes

| Code  | Bedeutung                              |
| ----- | -------------------------------------- |
| `0`   | Erfolg                                 |
| `1`   | Fehler (Meldung auf stderr)            |
| `130` | Abbruch mit Ctrl+C                     |

---

## Docker

Unter macOS, Windows oder ohne lokale Installation läuft das Tool in einem Kali-Container:

```bash
./scripts/run-docker.sh /cases/case01/disk.E01 "<recovery-key>" ./case01
```

Das Skript baut das Image, bindet den Ordner mit den Segmenten read-only ein und startet den Container mit `--privileged` (nötig für FUSE).

---

## Tests

Alle Tests laufen in Docker, also auch unter macOS:

```bash
./scripts/run-tests.sh
```

Das Skript führt drei Stufen aus:

1. **Unit-Tests** (`tests/unit_tests.c`, lokal unter Linux mit `sudo make test`) für jedes Modul, unter anderem Segmentnamen, mmls-Parser, Signaturerkennung, Merge mit Grenzfällen, Mounts und Aufräumen.
2. **Integrationstests** (`tests/integration_tests.sh`) mit echten BitLocker-Volumes. Dazu gehören GPT, MBR, 4K-Sektoren, BitLocker To Go, AES-CBC mit Elephant-Diffuser, mehrere BitLocker-Partitionen, EWF mit über 100 Segmenten, komprimiertes EWF und Blockgeräte. Geprüft werden außerdem fehlende Segmente, falsche Schlüssel, Sonderzeichen in Pfaden und Ctrl+C während der Abfrage und während des Kopierens. Jedes Ergebnis wird per SHA-256 mit einer unabhängig erzeugten Referenz verglichen.
3. **Host-Test** von `scripts/run-docker.sh`.

Die BitLocker-Testimages stammen aus der Testsuite von [cryptsetup](https://gitlab.com/cryptsetup/cryptsetup). Sie werden beim ersten Lauf heruntergeladen, per Prüfsumme verifiziert und nicht mit eingecheckt.

---

## Hinweise

- Root-Rechte sind nötig (`sudo`), weil `dislocker` und `ewfmount` über FUSE einhängen.
- Getestet unter Kali Linux (rolling) mit dislocker 0.7.3 und The Sleuth Kit 4.14, unter Ubuntu 22.04 mit dislocker 0.7.2 und The Sleuth Kit 4.11, außerdem über Docker Desktop unter macOS.
- Das Binary im GitHub-Release ist unter Ubuntu 22.04 gebaut und läuft auf allen Systemen ab glibc 2.34. Die Pakete aus den Voraussetzungen werden trotzdem benötigt.
- Unterstützt wird aktuell der Wiederherstellungsschlüssel (Recovery Password). Benutzerpasswort, BEK-Datei oder FVEK werden noch nicht angeboten.
- Freier Speicher im Ausgabeordner: etwa die Größe des Images. Bereiche, die nur Nullen enthalten, werden nicht physisch geschrieben.
- Ein vorhandenes `merged.dd` wird nie überschrieben.

---

## Lizenz

Dieses Projekt steht unter der [MIT License](LICENSE) © 2025 [yalmn](https://github.com/yalmn/)

---

## Release-Hinweise

**v2.0.0:** Stabilisierung. EWF-Verarbeitung über `ewfmount` mit Unterstützung mehrerer Segmente, Partitionserkennung über die BitLocker-Signatur (auch MBR, 4K-Sektoren und To Go), Merge ohne Zwischendateien, zuverlässiges Aushängen und Aufräumen, Ctrl+C-Behandlung, kein `system()` mehr, automatisierte Tests.

**v1.0.0:** Erste Veröffentlichung.

Geplant sind automatische Dateiextraktion und ein forensischer Report mit Hashwerten.
