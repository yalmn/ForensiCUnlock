#!/bin/bash

# Skript zum Installieren der Abhängigkeiten und Kompilieren der Anwendung

echo "[*] Installiere benötigte Abhängigkeiten (xmount, ewf-tools, dislocker)..."
# Paketliste herunterladen und installieren:
#  - xmount: zum Einhängen von Images
#  - ewf-tools: zum Konvertieren von EWF-Containern
#  - dislocker: zur BitLocker-Entschlüsselung
#  - dmsetup: zum Erstellen von Device-Mapper-Devices
#  - util-linux: enthält u. a. mount- und loop-Device-Tools
sudo apt install -y xmount ewf-tools dislocker dmsetup util-linux

echo "[*] Baue die Applikation..."
# Ins Verzeichnis über dem Skript wechseln (Projekt-Root)
cd ..
# Alte Binär- und Objektdateien entfernen
make clean
# Quellcode kompilieren und Binär erzeugen
make

echo "[*] Installation abgeschlossen. Anwendung bereit unter ./forensic_unlock"
