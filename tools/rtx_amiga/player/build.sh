#!/bin/sh
# Baut tone_test.c zu einem AmigaOS-Executable und packt es zusammen mit
# startup-sequence auf eine bootfaehige ADF-Diskette (tone_disk.adf).
set -eu
cd "$(dirname "$0")"

. ../env.sh
export PATH="$HOME/.local/bin:$PATH"

vc +aos68k tone_test.c -o tone_test_prog

rm -f tone_disk.adf
xdftool -f tone_disk.adf create
xdftool tone_disk.adf format Workbench
xdftool tone_disk.adf write tone_test_prog /
xdftool tone_disk.adf makedir S
xdftool tone_disk.adf write startup-sequence S/
xdftool tone_disk.adf boot install

echo "tone_disk.adf gebaut. Test mit ./verify.sh"
