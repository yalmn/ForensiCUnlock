#!/bin/bash

IMAGE_NAME="forensicunlock"
DOCKERFILE_PATH="docker/Dockerfile"

# Prüfen, ob Image bereits existiert
if ! docker image inspect $IMAGE_NAME >/dev/null 2>&1; then
    echo "[*] Docker-Image '$IMAGE_NAME' nicht gefunden. Baue jetzt..."
    docker build -t $IMAGE_NAME -f "$DOCKERFILE_PATH" .
    if [ $? -ne 0 ]; then
        echo "[!] Fehler beim Bauen des Docker-Images."
        exit 1
    fi
else
    echo "[✓] Docker-Image '$IMAGE_NAME' vorhanden."
fi

# Container starten mit privilegierten Rechten und gemounteten Devices
echo "[*] Starte Docker-Container..."
docker run -it --rm --privileged \
  -v /dev:/dev \
  -v /mnt:/mnt \
  $IMAGE_NAME
