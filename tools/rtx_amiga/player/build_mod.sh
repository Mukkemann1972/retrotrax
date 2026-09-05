#!/bin/sh
# Baut den echten 4-Kanal-Pattern-Player (mod_player.c, Phase 2b) und packt
# ihn zusammen mit einem eingebetteten Song auf eine bootfaehige ADF-Diskette
# (mod_disk.adf). Nutzung: ./build_mod.sh <song.mod>
set -eu
cd "$(dirname "$0")"

if [ $# -ne 1 ]; then
    echo "Nutzung: $0 <song.mod>  (z.B. von build/rtx_to_mod erzeugt)"
    exit 1
fi
SONG="$1"

. ../env.sh
export PATH="$HOME/.local/bin:$PATH"

python3 mod2c.py "$SONG" song_data.h
vc +aos68k mod_player.c -o mod_player_prog

rm -f mod_disk.adf
xdftool -f mod_disk.adf create
xdftool mod_disk.adf format Workbench
xdftool mod_disk.adf write mod_player_prog /
xdftool mod_disk.adf makedir S
xdftool mod_disk.adf write startup-sequence-mod S/startup-sequence
xdftool mod_disk.adf boot install

echo "mod_disk.adf gebaut (Song: $SONG). Test mit ./verify_mod.sh"
