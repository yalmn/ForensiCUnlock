#!/bin/bash

echo "[*] Installationsskript für ForensiCUnlock gestartet..."

# Verzeichnisse vorbereiten
echo "[1/3] Erstelle notwendige Mountverzeichnisse..."
sudo mkdir -p /mnt/output /mnt/xmount /mnt/bitlocker

# Benötigte Tools installieren
echo "[2/3] Prüfe und installiere benötigte Pakete (Debian/Ubuntu)..."
sudo apt update
sudo apt install -y xmount dislocker ddrescue sleuthkit build-essential

# Kompilieren
echo "[3/3] Baue das ForensiCUnlock-Projekt..."
make

# Hinweis zur Nutzung
echo "\n Setup abgeschlossen."
echo "  Starte das Tool z.B. mit:"
echo "    sudo ./forensic_unlock /dev/sdX <bitlocker-key>"
echo "  Oder führe 'scripts/cleanup.sh' nach der Analyse aus."
