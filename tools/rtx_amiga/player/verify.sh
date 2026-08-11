#!/bin/sh
# Startet tone_disk.adf headless in FS-UAE (Xvfb, keine echte Anzeige noetig),
# nimmt die Audioausgabe ueber OpenAL Softs Wave-Writer-Backend auf und prueft,
# ob ein echtes (nicht-stilles) Signal dabei rauskam. Nach dem Muster der
# bestehenden Host-Verifikations-Tools im Repo (rtx_cli pack, rtx_wasm
# native_test.cpp): Rueckgabewert 0 = Ton nachgewiesen, 1 = Stille/Fehler.
set -eu
cd "$(dirname "$0")"

DISP=":89"
OUT="verify_out.wav"
rm -f "$OUT"

Xvfb "$DISP" -screen 0 640x480x24 &
XVFB_PID=$!
trap 'kill $XVFB_PID 2>/dev/null || true' EXIT
sleep 1

cat > /tmp/rtx_amiga_alsoft.conf << EOF
[general]
drivers=wave,

[wave]
file=$(pwd)/$OUT
EOF

ALSOFT_CONF=/tmp/rtx_amiga_alsoft.conf DISPLAY="$DISP" \
    timeout 30 fs-uae --kickstart_file=internal --amiga_model=A500 \
    --fullscreen=0 --floppy_drive_0=tone_disk.adf > /tmp/rtx_amiga_fsuae.log 2>&1 || true

if [ ! -s "$OUT" ]; then
    echo "FEHLER: keine Audiodatei erzeugt (siehe /tmp/rtx_amiga_fsuae.log)"
    exit 1
fi

PEAK_DB=$(ffmpeg -i "$OUT" -af astats -f null - 2>&1 \
    | grep "Peak level dB" | tail -1 | sed -E 's/.*: //')

echo "Peak level: ${PEAK_DB} dB (Datei: $OUT)"
# grobe Schwelle: -60 dB gilt als Stille (Rauschen/Rundung), alles darueber
# ist ein echtes Signal
awk -v db="$PEAK_DB" 'BEGIN { exit !(db > -60) }' \
    && { echo "OK: Signal nachgewiesen."; exit 0; } \
    || { echo "FEHLER: nur Stille."; exit 1; }
