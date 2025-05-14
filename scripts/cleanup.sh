#!/bin/bash

echo "[*] Cleanup gestartet..."

# Umounts
echo "→ Versuche /mnt/output zu unmounten..."
umount /mnt/output 2>/dev/null

echo "→ Versuche /mnt/xmount zu unmounten..."
umount /mnt/xmount 2>/dev/null

echo "→ Versuche /mnt/bitlocker zu unmounten..."
umount /mnt/bitlocker 2>/dev/null

# Loop-Devices entfernen
echo "→ Entferne zugehörige Loop-Devices..."
for loopdev in $(losetup -a | grep -E 'image.dd|dislocker-file' | cut -d: -f1); do
  echo "  - Entferne $loopdev"
  losetup -d "$loopdev"
done

# Device Mapper entfernen
echo "→ Entferne /dev/mapper/merged (falls vorhanden)..."
dmsetup remove merged 2>/dev/null

echo "[✓] Cleanup abgeschlossen."
