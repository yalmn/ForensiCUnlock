#!/bin/bash

echo "[*] Installiere benötigte Abhängigkeiten (xmount, ewf-tools, dislocker)..."
sudo apt install -y xmount ewf-tools dislocker dmsetup util-linux

echo "[*] Baue die Applikation..."
cd ..
make clean
make

echo "[✔] Installation abgeschlossen. Anwendung bereit unter ./forensic_unlock"
