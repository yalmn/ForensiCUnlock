#!/bin/bash
# Führt alle Tests aus (funktioniert unter Linux, macOS und Windows mit Docker):
#   1. Unit-Tests im Kali-Container
#   2. Integrationstests mit echten BitLocker-Images im Container
#   3. scripts/run-docker.sh vom Host aus
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
HOST_DIR="tests/fixtures/host-test"

sha() {
    if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

echo "[*] Baue Test-Image..."
docker build -q --target test -t forensicunlock-test -f docker/Dockerfile . >/dev/null

echo "[*] Starte Unit- und Integrationstests im Container..."
docker run --rm --privileged -v "$ROOT":/opt/forensicunlock forensicunlock-test bash -c "
    set -e
    make clean >/dev/null
    make test
    EXPORT_DIR=/opt/forensicunlock/$HOST_DIR ./tests/integration_tests.sh
    make clean >/dev/null
"

echo
echo "[*] Teste scripts/run-docker.sh vom Host aus..."
rm -rf "$HOST_DIR/ausgabe"
printf '\n' | ./scripts/run-docker.sh "$HOST_DIR/ewf/fall.e01" "$(cat "$HOST_DIR/key")" "$HOST_DIR/ausgabe" >"$HOST_DIR/run-docker.log" 2>&1 || {
    cat "$HOST_DIR/run-docker.log"
    echo "run-docker.sh: FEHLER (Exit-Code)"
    exit 1
}
if [ "$(sha "$HOST_DIR/ausgabe/merged.dd")" != "$(sha "$HOST_DIR/expected.dd")" ]; then
    echo "run-docker.sh: FEHLER (merged.dd weicht ab)"
    exit 1
fi
if [ -e "$HOST_DIR/ausgabe/ewf" ] || [ -e "$HOST_DIR/ausgabe/bitlocker" ]; then
    echo "run-docker.sh: FEHLER (Hilfsordner nicht entfernt)"
    exit 1
fi
echo "run-docker.sh: ok"
rm -rf "$HOST_DIR"

echo
echo "[+] Alle Tests bestanden."
