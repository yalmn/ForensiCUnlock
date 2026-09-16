#!/bin/bash
# Startet ForensiCUnlock in einem Kali-Container. Praktisch unter macOS, Windows
# oder wenn dislocker und ewf-tools nicht lokal installiert werden sollen.
#
#   ./scripts/run-docker.sh <image|device> <recovery-key> <ausgabeordner>
set -eu

if [ $# -ne 3 ]; then
    echo "Verwendung: $0 <image|device> <recovery-key> <ausgabeordner>" >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KEY="$2"

if [ ! -e "$1" ]; then
    echo "[!] Eingabe nicht gefunden: $1" >&2
    exit 1
fi
mkdir -p "$3"

# Absolute Pfade, damit sie im Container gleich aussehen
INPUT="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUTPUT="$(cd "$3" && pwd)"

echo "[*] Baue Docker-Image forensicunlock..."
docker build -q -t forensicunlock -f "$ROOT/docker/Dockerfile" "$ROOT" >/dev/null

MOUNTS=(-v "$OUTPUT":"$OUTPUT")
if [ ! -b "$INPUT" ]; then
    # Ganzes Verzeichnis einbinden, damit alle EWF-Segmente (E01, E02, ...) sichtbar sind
    INPUT_DIR="$(dirname "$INPUT")"
    MOUNTS+=(-v "$INPUT_DIR":"$INPUT_DIR":ro)
fi

TTY=(-i)
[ -t 0 ] && [ -t 1 ] && TTY=(-it)

echo "[*] Starte ForensiCUnlock im Container..."
exec docker run --rm "${TTY[@]}" --privileged "${MOUNTS[@]}" forensicunlock "$INPUT" "$KEY" "$OUTPUT"
