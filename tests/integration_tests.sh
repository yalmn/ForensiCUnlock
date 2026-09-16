#!/bin/bash
# Integrationstests für forensic_unlock mit echten BitLocker-Images.
#
# Voraussetzungen: Linux, root, FUSE (im Container: --privileged),
# dislocker, ewf-tools, sleuthkit, fdisk (sfdisk), curl, xz-utils.
#
# Die BitLocker-Testimages stammen aus der Testsuite von cryptsetup
# (https://gitlab.com/cryptsetup/cryptsetup, tests/bitlk-images.tar.xz).
# Sie werden beim ersten Lauf heruntergeladen und nach tests/fixtures/ entpackt.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/forensic_unlock"
FIXTURES="${FIXTURES:-$ROOT/tests/fixtures}"
# Arbeitsordner muss Groß-/Kleinschreibung unterscheiden (nicht auf einem macOS-Bind-Mount)
WORK="${WORK:-/tmp/forensicunlock-integration}"

FIXTURE_COMMIT="4eb729da3f46642d6fe1fabbbedb127078eccb95"
FIXTURE_URL="https://gitlab.com/cryptsetup/cryptsetup/-/raw/$FIXTURE_COMMIT/tests/bitlk-images.tar.xz"
FIXTURE_SHA256="68bf5669f777668112d497234ebe2166b5feaf0e33b206b095b917f82937bd30"
IMAGES="$FIXTURES/bitlk-images"

MIB=$((1024 * 1024))
PASSED=0
FAILED=0
FAILED_NAMES=()

# Hilfsfunktionen

log() { printf '\n=== %s\n' "$*"; }

check() {
    local desc="$1"
    shift
    if "$@"; then
        PASSED=$((PASSED + 1))
        printf '  ok     %s\n' "$desc"
    else
        FAILED=$((FAILED + 1))
        FAILED_NAMES+=("$CURRENT_TEST: $desc")
        printf '  FEHLER %s\n' "$desc"
    fi
}

die() {
    echo "[!] $*" >&2
    exit 2
}

# Wiederherstellungsschlüssel eines Testimages aus images.conf
recovery_key() {
    awk -v sect="[$1]" '$0 == sect { found = 1; next } /^\[/ { found = 0 } found && /^RP=/ { sub(/^RP=/, ""); print; exit }' \
        "$IMAGES/images.conf"
}

sha() { sha256sum "$1" | cut -d' ' -f1; }

same_file() { [ -f "$1" ] && [ -f "$2" ] && [ "$(sha "$1")" = "$(sha "$2")" ]; }

no_mounts_below() { ! grep -qF " $1" /proc/mounts; }

log_contains() { grep -qF -- "$2" "$1"; }

not_exists() { [ ! -e "$1" ]; }

bytes_at() { dd if="$1" bs=1 skip="$2" count="$3" status=none; }

# Legt ein Datenträger-Image mit Partitionstabelle an.
#   make_disk <ziel> <gpt|dos> <sektorgröße> <partition>...
# Partitionen:
#   fill:<MiB>         Zufallsdaten
#   bl:<testimage>     BitLocker-Volume aus den Fixtures
#   blname:<testimage> wie bl, bei GPT mit Namen "Basic data partition"
# Die Byte-Offsets der Partitionen landen in <ziel>.offsets (eine Zeile je Partition).
make_disk() {
    local out="$1" label="$2" ss="$3"
    shift 3
    local specs=("$@") sizes=() total=$MIB script="label: $label"$'\n'

    [ "$label" = gpt ] && script+="first-lba: $((MIB / ss))"$'\n'
    for spec in "${specs[@]}"; do
        local kind="${spec%%:*}" arg="${spec#*:}" size
        case "$kind" in
        fill) size=$((arg * MIB)) ;;
        bl | blname) size=$(stat -c %s "$IMAGES/$arg.img") ;;
        *) die "Unbekannte Partitionsart $spec" ;;
        esac
        sizes+=("$size")
        total=$((total + size))
    done
    total=$((total + 2 * MIB)) # Platz für GPT-Backup und nicht zugeordneten Rest

    rm -f "$out" "$out.offsets"
    truncate -s "$total" "$out"

    local offset=$MIB i=0
    for spec in "${specs[@]}"; do
        local kind="${spec%%:*}" size="${sizes[$i]}" line
        line="start=$((offset / ss)), size=$((size / ss))"
        if [ "$label" = gpt ]; then
            line+=", type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"
            [ "$kind" = blname ] && line+=', name="Basic data partition"'
        else
            [ "$kind" = fill ] && line+=", type=83" || line+=", type=7"
        fi
        script+="$line"$'\n'
        echo "$offset" >>"$out.offsets"
        offset=$((offset + size))
        i=$((i + 1))
    done
    if [ "$ss" = 512 ]; then
        printf '%s' "$script" | sfdisk -q "$out" >/dev/null || die "sfdisk fehlgeschlagen für $out"
    else
        # Ältere sfdisk-Versionen kennen --sector-size nicht, daher über ein Loop-Device mit passender Sektorgröße
        local loop
        loop="$(losetup --find --show --sector-size "$ss" "$out")" || die "losetup fehlgeschlagen für $out"
        printf '%s' "$script" | sfdisk -q "$loop" >/dev/null
        local rc=$?
        losetup -d "$loop"
        [ "$rc" = 0 ] || die "sfdisk fehlgeschlagen für $out"
    fi

    offset=$MIB
    i=0
    for spec in "${specs[@]}"; do
        local kind="${spec%%:*}" arg="${spec#*:}" size="${sizes[$i]}"
        if [ "$kind" = fill ]; then
            head -c "$size" /dev/urandom | dd of="$out" bs=1M oflag=seek_bytes seek="$offset" conv=notrunc status=none
        else
            dd if="$IMAGES/$arg.img" of="$out" bs=1M oflag=seek_bytes seek="$offset" conv=notrunc status=none
        fi
        offset=$((offset + size))
        i=$((i + 1))
    done
    # Auch den nicht zugeordneten Bereich zwischen MBR/GPT und erster Partition mit Daten füllen
    head -c $((MIB / 2)) /dev/urandom | dd of="$out" bs=1M oflag=seek_bytes seek=$((MIB / 2)) conv=notrunc status=none
}

# Erwartetes Ergebnis unabhängig vom Tool erzeugen: dislocker direkt auf das Testimage,
# dann die entschlüsselten Bytes an die Stelle der Partition schreiben.
#   make_expected <disk> <ziel> <partitionsnummer ab 1> <testimage>
make_expected() {
    local disk="$1" out="$2" index="$3" name="$4" offset mnt
    offset=$(sed -n "${index}p" "$disk.offsets")
    mnt="$WORK/expected-mnt"
    mkdir -p "$mnt"
    dislocker -V "$IMAGES/$name.img" -p"$(recovery_key "$name")" -r -- "$mnt" >/dev/null 2>&1 ||
        die "dislocker konnte $name nicht öffnen"
    cp --sparse=always "$disk" "$out"
    dd if="$mnt/dislocker-file" of="$out" bs=1M oflag=seek_bytes seek="$offset" conv=notrunc status=none
    fusermount -u "$mnt"
    rmdir "$mnt"
}

# Führt das Tool aus. Eingaben für stdin kommen aus $INPUT (Standard: ENTER).
#   run_tool <logdatei> <argumente...>   Exit-Code landet in $RC
run_tool() {
    local logfile="$1"
    shift
    printf '%b' "${INPUT-\n}" | "$BIN" "$@" >"$logfile" 2>&1
    RC=$?
}

# Vorbereitung

[ "$(id -u)" = 0 ] || die "Die Integrationstests müssen als root laufen."
for tool in dislocker ewfmount ewfacquire mmls sfdisk curl xz fusermount setpriv; do
    command -v "$tool" >/dev/null || die "Programm fehlt: $tool"
done
[ -x "$BIN" ] || die "$BIN fehlt, bitte zuerst make ausführen."

if [ ! -f "$IMAGES/images.conf" ]; then
    echo "[*] Lade BitLocker-Testimages von cryptsetup..."
    mkdir -p "$FIXTURES"
    curl -fsSL -o "$FIXTURES/bitlk-images.tar.xz" "$FIXTURE_URL" || die "Download fehlgeschlagen"
    [ "$(sha "$FIXTURES/bitlk-images.tar.xz")" = "$FIXTURE_SHA256" ] || die "Prüfsumme der Testimages stimmt nicht"
    tar -xJf "$FIXTURES/bitlk-images.tar.xz" -C "$FIXTURES" || die "Entpacken fehlgeschlagen"
fi

rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 2

echo "[*] Erzeuge Test-Datenträger..."
make_disk gpt.dd gpt 512 fill:8 blname:bitlk-aes-xts-128 fill:8
make_expected gpt.dd gpt.expected 2 bitlk-aes-xts-128
make_disk mbr.dd dos 512 fill:4 fill:4 bl:bitlk-aes-cbc-128 fill:8
make_expected mbr.dd mbr.expected 3 bitlk-aes-cbc-128
make_disk gpt4k.dd gpt 4096 fill:8 bl:bitlk-aes-xts-128-4k fill:8
make_expected gpt4k.dd gpt4k.expected 2 bitlk-aes-xts-128-4k
make_disk togo.dd dos 512 bl:bitlk-togo-aes-xts-128 fill:8
make_expected togo.dd togo.expected 1 bitlk-togo-aes-xts-128
make_disk elephant.dd gpt 512 bl:bitlk-aes-cbc-elephant-128
make_expected elephant.dd elephant.expected 1 bitlk-aes-cbc-elephant-128
make_disk zwei.dd gpt 512 fill:4 bl:bitlk-aes-xts-128 fill:4 bl:bitlk-aes-xts-256
make_expected zwei.dd zwei.expected 4 bitlk-aes-xts-256
make_disk ohne.dd gpt 512 fill:8 fill:8

RP_XTS128="$(recovery_key bitlk-aes-xts-128)"

# Tests

CURRENT_TEST="GPT, 512-Byte-Sektoren, Partitionsname vorhanden"
log "$CURRENT_TEST"
run_tool gpt.log gpt.dd "$RP_XTS128" out-gpt
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file out-gpt/merged.dd gpt.expected
check "entschlüsselter Bootsektor ist NTFS" [ "$(bytes_at out-gpt/merged.dd $((9 * MIB + 3)) 8)" = "NTFS    " ]
check "Bereich vor der Partition unverändert" \
    cmp -s <(head -c $((9 * MIB)) gpt.dd) <(head -c $((9 * MIB)) out-gpt/merged.dd)
check "Größe identisch mit Original" [ "$(stat -c %s gpt.dd)" = "$(stat -c %s out-gpt/merged.dd)" ]
check "bdp.info korrekt" grep -qx "offset_bytes=$((9 * MIB))" out-gpt/bdp.info
check "keine Hilfsordner übrig" not_exists out-gpt/bitlocker
check "kein ewf-Ordner übrig" not_exists out-gpt/ewf
check "keine Mounts übrig" no_mounts_below "$WORK"
check "Schlüssel nicht im Log" bash -c "! grep -qF '$RP_XTS128' gpt.log"
check "Partitionsname wird angezeigt" log_contains gpt.log "Basic data partition"

CURRENT_TEST="MBR mit NTFS-Typ 0x07 und AES-CBC"
log "$CURRENT_TEST"
run_tool mbr.log mbr.dd "$(recovery_key bitlk-aes-cbc-128)" out-mbr
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file out-mbr/merged.dd mbr.expected
check "Linux-Partitionen werden nicht als BitLocker erkannt" bash -c "! grep -q 'Mehrere BitLocker' mbr.log"

CURRENT_TEST="GPT mit 4K-Sektoren"
log "$CURRENT_TEST"
run_tool gpt4k.log gpt4k.dd "$(recovery_key bitlk-aes-xts-128-4k)" out-4k
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file out-4k/merged.dd gpt4k.expected
check "Sektorgröße 4096 erkannt" grep -qx "sector_size=4096" out-4k/bdp.info

CURRENT_TEST="BitLocker To Go"
log "$CURRENT_TEST"
run_tool togo.log togo.dd "$(recovery_key bitlk-togo-aes-xts-128)" out-togo
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file out-togo/merged.dd togo.expected
check "entschlüsselter Bootsektor ist FAT" [ "$(bytes_at out-togo/merged.dd $((MIB + 3)) 8)" = "MSDOS5.0" ]

CURRENT_TEST="AES-CBC mit Elephant-Diffuser, Partition direkt am Imageanfang"
log "$CURRENT_TEST"
run_tool elephant.log elephant.dd "$(recovery_key bitlk-aes-cbc-elephant-128)" out-elephant
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file out-elephant/merged.dd elephant.expected

CURRENT_TEST="Mehrere BitLocker-Partitionen mit Auswahl"
log "$CURRENT_TEST"
INPUT='abc\n7\n2\n\n' run_tool zwei.log zwei.dd "$(recovery_key bitlk-aes-xts-256)" out-zwei
check "Exit-Code 0" [ "$RC" = 0 ]
check "Auswahl wurde angeboten" log_contains zwei.log "Mehrere BitLocker-Partitionen gefunden"
check "ungültige Eingaben werden abgewiesen" [ "$(grep -c 'Ungültige Eingabe' zwei.log)" = 2 ]
check "zweite Partition entschlüsselt" same_file out-zwei/merged.dd zwei.expected

INPUT='' run_tool zwei-eof.log zwei.dd "$(recovery_key bitlk-aes-xts-256)" out-zwei-eof
check "EOF bei der Auswahl bricht ab" [ "$RC" = 1 ]
check "Ausgabeordner wieder entfernt" not_exists out-zwei-eof

CURRENT_TEST="EWF mit mehr als 100 Segmenten (E01 bis EAx)"
log "$CURRENT_TEST"
mkdir -p ewf-viele
ewfacquire -u -t ewf-viele/fall -S 1MiB -c none gpt.dd >ewfacquire.log 2>&1 || die "ewfacquire fehlgeschlagen"
check "Testdaten enthalten .EAA" [ -f ewf-viele/fall.EAA ]
run_tool ewf.log ewf-viele/fall.E01 "$RP_XTS128" out-ewf
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd identisch mit RAW-Ergebnis" same_file out-ewf/merged.dd gpt.expected
check "Segmentanzahl im Log" log_contains ewf.log "EWF-Image mit $(ls ewf-viele | wc -l) Segment(en)"
check "ewf-Ordner entfernt" not_exists out-ewf/ewf
check "keine Mounts übrig" no_mounts_below "$WORK"

CURRENT_TEST="EWF mit wenigen Segmenten, Kleinbuchstaben-Endung"
log "$CURRENT_TEST"
mkdir -p ewf-klein
ewfacquire -u -t ewf-klein/fall -S 20MiB -c none gpt.dd >>ewfacquire.log 2>&1 || die "ewfacquire fehlgeschlagen"
for f in ewf-klein/fall.E0*; do mv "$f" "${f%.E0*}.e0${f##*.E0}" || die "Umbenennen fehlgeschlagen"; done
check "Testdaten enthalten mindestens .e01 bis .e04" [ -f ewf-klein/fall.e04 ]
run_tool ewf-klein.log ewf-klein/fall.e01 "$RP_XTS128" out-ewf-klein
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd identisch mit RAW-Ergebnis" same_file out-ewf-klein/merged.dd gpt.expected

CURRENT_TEST="EWF komprimiert"
log "$CURRENT_TEST"
mkdir -p ewf-komprimiert
ewfacquire -u -t ewf-komprimiert/fall -c best gpt.dd >>ewfacquire.log 2>&1 || die "ewfacquire fehlgeschlagen"
check "Segment ist kleiner als das Original" [ "$(stat -c %s ewf-komprimiert/fall.E01)" -lt "$(stat -c %s gpt.dd)" ]
run_tool ewf-komprimiert.log ewf-komprimiert/fall.E01 "$RP_XTS128" out-ewf-komprimiert
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd identisch mit RAW-Ergebnis" same_file out-ewf-komprimiert/merged.dd gpt.expected

CURRENT_TEST="EWF mit fehlendem Segment"
log "$CURRENT_TEST"
cp -r ewf-klein ewf-luecke
rm ewf-luecke/fall.e02 || die "Testvorbereitung fehlgeschlagen"
run_tool luecke.log ewf-luecke/fall.e01 "$RP_XTS128" out-luecke
check "Exit-Code 1" [ "$RC" = 1 ]
check "Meldung nennt fehlendes Segment" log_contains luecke.log "Segment .e02 fehlt"
check "Ausgabeordner wieder entfernt" not_exists out-luecke
check "keine Mounts übrig" no_mounts_below "$WORK"

CURRENT_TEST="EWF: falsches Segment angegeben"
log "$CURRENT_TEST"
run_tool e02.log ewf-klein/fall.e02 "$RP_XTS128" out-e02
check "Exit-Code 1" [ "$RC" = 1 ]
check "Hinweis auf erstes Segment" log_contains e02.log "Bitte das erste Segment angeben"

CURRENT_TEST="Falscher Wiederherstellungsschlüssel"
log "$CURRENT_TEST"
run_tool falsch.log gpt.dd "111111-111111-111111-111111-111111-111111-111111-111111" out-falsch
check "Exit-Code 1" [ "$RC" = 1 ]
check "Fehlermeldung" log_contains falsch.log "dislocker konnte das Volume nicht entschlüsseln"
check "Ausgabeordner wieder entfernt" not_exists out-falsch
check "keine Mounts übrig" no_mounts_below "$WORK"

run_tool falsch-ewf.log ewf-klein/fall.e01 "111111-111111-111111-111111-111111-111111-111111-111111" out-falsch-ewf
check "EWF: Exit-Code 1" [ "$RC" = 1 ]
check "EWF: keine Mounts übrig" no_mounts_below "$WORK"
check "EWF: Ausgabeordner wieder entfernt" not_exists out-falsch-ewf

CURRENT_TEST="Image ohne BitLocker"
log "$CURRENT_TEST"
run_tool ohne.log ohne.dd "$RP_XTS128" out-ohne
check "Exit-Code 1" [ "$RC" = 1 ]
check "Meldung" log_contains ohne.log "Keine BitLocker-Partition gefunden"

truncate -s 10M leer.dd
run_tool leer.log leer.dd "$RP_XTS128" out-leer
check "ohne Partitionstabelle: Exit-Code 1" [ "$RC" = 1 ]
check "ohne Partitionstabelle: Meldung" log_contains leer.log "mmls konnte keine Partitionstabelle lesen"

CURRENT_TEST="Vorhandenes Ergebnis wird nicht überschrieben"
log "$CURRENT_TEST"
mkdir -p out-vorhanden
echo "alte Daten" >out-vorhanden/merged.dd
run_tool vorhanden.log gpt.dd "$RP_XTS128" out-vorhanden
check "Exit-Code 1" [ "$RC" = 1 ]
check "alte Datei unverändert" grep -qx "alte Daten" out-vorhanden/merged.dd

CURRENT_TEST="Belegter Mountpunkt im Ausgabeordner"
log "$CURRENT_TEST"
mkdir -p out-belegt/bitlocker
mount -t tmpfs tmpfs out-belegt/bitlocker
run_tool belegt.log gpt.dd "$RP_XTS128" out-belegt
check "Exit-Code 1" [ "$RC" = 1 ]
check "Meldung" log_contains belegt.log "noch etwas eingehängt"
umount out-belegt/bitlocker

CURRENT_TEST="Leerzeichen und Anführungszeichen in Pfaden"
log "$CURRENT_TEST"
mkdir -p "pfad mit 'quote' und \$(x)"
cp --sparse=always gpt.dd "pfad mit 'quote' und \$(x)/disk; echo.dd"
run_tool sonderzeichen.log "pfad mit 'quote' und \$(x)/disk; echo.dd" "$RP_XTS128" "ausgabe mit 'quote' \$(x)"
check "Exit-Code 0" [ "$RC" = 0 ]
check "merged.dd entspricht der Referenz" same_file "ausgabe mit 'quote' \$(x)/merged.dd" gpt.expected

CURRENT_TEST="Ohne Ausgabeordner: run_<zeitstempel>"
log "$CURRENT_TEST"
mkdir -p ohne-ordner
(cd ohne-ordner && printf '\n' | "$BIN" ../gpt.dd "$RP_XTS128" >../timestamp.log 2>&1)
RC=$?
check "Exit-Code 0" [ "$RC" = 0 ]
RUN_DIR="$(ls -d ohne-ordner/run_* 2>/dev/null | head -1)"
check "Ordner run_JJJJMMTT_HHMMSS angelegt" bash -c "[[ '$RUN_DIR' =~ ohne-ordner/run_[0-9]{8}_[0-9]{6}$ ]]"
check "merged.dd entspricht der Referenz" same_file "$RUN_DIR/merged.dd" gpt.expected

CURRENT_TEST="Blockgerät als Eingabe"
log "$CURRENT_TEST"
LOOPDEV="$(losetup --find --show --read-only gpt.dd 2>/dev/null)"
if [ -n "$LOOPDEV" ]; then
    run_tool loop.log "$LOOPDEV" "$RP_XTS128" out-loop
    check "Exit-Code 0" [ "$RC" = 0 ]
    check "merged.dd entspricht der Referenz" same_file out-loop/merged.dd gpt.expected
    losetup -d "$LOOPDEV"
else
    check "Loop-Device verfügbar" false
fi

CURRENT_TEST="Aufruf- und Rechteprüfung"
log "$CURRENT_TEST"
"$BIN" >usage.log 2>&1
RC=$?
check "ohne Argumente: Exit-Code 1" [ "$RC" = 1 ]
check "ohne Argumente: Hilfe" log_contains usage.log "Verwendung:"
"$BIN" a b c d >usage2.log 2>&1
check "zu viele Argumente: Exit-Code 1" [ "$?" = 1 ]
setpriv --reuid=65534 --regid=65534 --clear-groups "$BIN" gpt.dd "$RP_XTS128" out-user >user.log 2>&1
check "ohne root: Exit-Code 1" [ "$?" = 1 ]
check "ohne root: Meldung" log_contains user.log "root-Rechten"
run_tool fehlt.log gibt-es-nicht.dd "$RP_XTS128" out-fehlt
check "fehlende Eingabe: Exit-Code 1" [ "$RC" = 1 ]
check "fehlende Eingabe: kein Ausgabeordner" not_exists out-fehlt

CURRENT_TEST="Abbruch an der Bestätigung"
log "$CURRENT_TEST"
INPUT='' run_tool eof.log ewf-klein/fall.e01 "$RP_XTS128" out-eof
check "EOF: Exit-Code 1" [ "$RC" = 1 ]
check "EOF: EWF ausgehängt" no_mounts_below "$WORK"
check "EOF: Ausgabeordner entfernt" not_exists out-eof

# Ctrl+C während das Tool auf ENTER wartet (EWF ist zu dem Zeitpunkt eingehängt)
mkfifo eingabe.fifo
"$BIN" ewf-klein/fall.e01 "$RP_XTS128" out-sigint <eingabe.fifo >sigint.log 2>&1 &
PID=$!
exec 3>eingabe.fifo
for _ in $(seq 100); do log_contains sigint.log "ENTER zum Fortfahren" && break; sleep 0.1; done
check "EWF während der Abfrage eingehängt" grep -qF " $WORK/out-sigint/ewf" /proc/mounts
kill -INT "$PID"
wait "$PID"
RC=$?
exec 3>&-
check "Ctrl+C: Exit-Code 130" [ "$RC" = 130 ]
check "Ctrl+C: Meldung" log_contains sigint.log "Abgebrochen"
check "Ctrl+C: keine Mounts übrig" no_mounts_below "$WORK"
check "Ctrl+C: Ausgabeordner entfernt" not_exists out-sigint

CURRENT_TEST="Abbruch während des Zusammenführens"
log "$CURRENT_TEST"
make_disk gross.dd gpt 512 fill:768 bl:bitlk-aes-xts-128 fill:8
mkfifo eingabe2.fifo
"$BIN" gross.dd "$RP_XTS128" out-merge-abbruch <eingabe2.fifo >merge-abbruch.log 2>&1 &
PID=$!
exec 3>eingabe2.fifo
for _ in $(seq 100); do log_contains merge-abbruch.log "ENTER zum Fortfahren" && break; sleep 0.1; done
echo >&3
for _ in $(seq 400); do log_contains merge-abbruch.log "Kopiere Bereich vor" && break; sleep 0.01; done
kill -STOP "$PID" 2>/dev/null
check "Abbruch erfolgt während des Kopierens" bash -c "! grep -q 'Kopiere Bereich nach' merge-abbruch.log"
check "dislocker während des Kopierens eingehängt" grep -qF " $WORK/out-merge-abbruch/bitlocker" /proc/mounts
kill -INT "$PID" 2>/dev/null
kill -CONT "$PID" 2>/dev/null
wait "$PID"
RC=$?
exec 3>&-
check "Exit-Code 130" [ "$RC" = 130 ]
check "halbes merged.dd gelöscht" not_exists out-merge-abbruch/merged.dd
check "keine Mounts übrig" no_mounts_below "$WORK"
check "Ausgabeordner entfernt" not_exists out-merge-abbruch
rm -f gross.dd

# Testdaten für den Host-Test von scripts/run-docker.sh bereitstellen
if [ -n "${EXPORT_DIR:-}" ]; then
    mkdir -p "$EXPORT_DIR"
    rm -rf "$EXPORT_DIR"/*
    cp gpt.expected "$EXPORT_DIR/expected.dd"
    cp -r ewf-klein "$EXPORT_DIR/ewf"
    echo "$RP_XTS128" >"$EXPORT_DIR/key"
fi

# Ergebnis

echo
echo "Integrationstests: $PASSED bestanden, $FAILED fehlgeschlagen"
if [ "$FAILED" -gt 0 ]; then
    printf '  - %s\n' "${FAILED_NAMES[@]}"
    echo "Logs liegen in $WORK"
    exit 1
fi
exit 0
