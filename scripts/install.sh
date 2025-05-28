#!/bin/bash

echo "[*] Installiere benötigte Pakete (dislocker, xmount, sleuthkit, parted)..."
sudo apt update
sudo apt install -y dislocker xmount sleuthkit parted build-essential

echo "[*] Starte Build-Vorgang..."
make clean && make

if [[ -f forensic_unlock ]]; then
    echo "[+] Build erfolgreich abgeschlossen."
    echo "[+] Starte mit: sudo ./forensic_unlock <image> <key> <ausgabeverzeichnis>"
else
    echo "[!] Fehler beim Kompilieren von forensic_unlock."
    exit 1
fi
