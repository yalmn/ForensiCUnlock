#!/bin/bash
# Installiert die Abhängigkeiten (Debian, Ubuntu, Kali, WSL2) und baut forensic_unlock.
set -eu

cd "$(dirname "$0")/.."

echo "[*] Installiere benötigte Pakete (dislocker, ewf-tools, sleuthkit, fuse3)..."
sudo apt-get update
sudo apt-get install -y build-essential dislocker ewf-tools sleuthkit fuse3

echo "[*] Starte Build-Vorgang..."
make clean
make

echo "[+] Build erfolgreich abgeschlossen."
echo "[+] Starte mit: sudo ./forensic_unlock <image> <recovery-key> [ausgabeordner]"
