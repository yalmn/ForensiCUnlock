#!/bin/bash

DEVICE=$1
KEY=$2
OUTPUT=$3

if [ $# -ne 3 ]; then
    echo "Usage: $0 <device> <bitlocker_key> <output_folder>"
    exit 1
fi

echo "[*] Starte ForensiCUnlock im Docker-Container..."
docker run --rm -it \
  --privileged \
  -v "${DEVICE}":"${DEVICE}" \
  -v "${OUTPUT}":"${OUTPUT}" \
  -v "$(pwd)":/tool \
  forensicunlock \
  bash -c "cd /tool && sudo ./forensic_unlock '${DEVICE}' '${KEY}' '${OUTPUT}'"
