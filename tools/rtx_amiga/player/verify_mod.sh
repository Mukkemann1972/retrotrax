#!/bin/sh
# Startet mod_disk.adf headless in FS-UAE (wie verify.sh aus Phase 1), nimmt
# die Audioausgabe auf und prueft auf ein echtes (nicht-stilles) Signal. Der
# Player laeuft endlos (echter Hintergrundmusik-Player) - darum per timeout
# nach ein paar Sekunden hart beendet, das reicht fuer den Nachweis.
set -eu
cd "$(dirname "$0")"

DISP=":90"
OUT="verify_mod_out.wav"
rm -f "$OUT"

Xvfb "$DISP" -screen 0 640x480x24 &
XVFB_PID=$!
trap 'kill $XVFB_PID 2>/dev/null || true' EXIT
sleep 1

cat > /tmp/rtx_amiga_mod_alsoft.conf << EOF
[general]
drivers=wave,

[wave]
file=$(pwd)/$OUT
EOF

ALSOFT_CONF=/tmp/rtx_amiga_mod_alsoft.conf DISPLAY="$DISP" \
    timeout 15 fs-uae --kickstart_file=internal --amiga_model=A500 \
    --fullscreen=0 --floppy_drive_0=mod_disk.adf > /tmp/rtx_amiga_mod_fsuae.log 2>&1 || true

if [ ! -s "$OUT" ]; then
    echo "FEHLER: keine Audiodatei erzeugt (siehe /tmp/rtx_amiga_mod_fsuae.log)"
    exit 1
fi

DUR=$(ffmpeg -i "$OUT" -f null - 2>&1 | grep -o "time=[0-9:.]*" | tail -1)
PEAK_DB=$(ffmpeg -i "$OUT" -af astats -f null - 2>&1 \
    | grep "Peak level dB" | tail -1 | sed -E 's/.*: //')

echo "Dauer: ${DUR}, Peak level: ${PEAK_DB} dB (Datei: $OUT)"
awk -v db="$PEAK_DB" 'BEGIN { exit !(db > -60) }' \
    && { echo "OK: Signal nachgewiesen."; exit 0; } \
    || { echo "FEHLER: nur Stille."; exit 1; }
