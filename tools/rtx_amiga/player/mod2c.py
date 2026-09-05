#!/usr/bin/env python3
"""Baut aus einem klassischen .mod (z.B. von rtx_to_mod) einen C-Header mit dem
kompletten Song als eingebettetes Byte-Array - der 68k-Player braucht kein
AmigaDOS-Dateisystem, der Song steckt direkt im Executable (wie schon Phase 1s
eingebettetes Testsample in tone_test.c).

Nutzung: mod2c.py song.mod song_data.h
"""
import sys


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <song.mod> <song_data.h>")
        sys.exit(1)
    mod_path, out_path = sys.argv[1], sys.argv[2]
    with open(mod_path, "rb") as f:
        data = f.read()

    with open(out_path, "w") as f:
        f.write("/* Automatisch erzeugt von mod2c.py aus %s - nicht von Hand bearbeiten. */\n"
                % mod_path)
        f.write("#define SONG_DATA_LEN %dUL\n" % len(data))
        f.write("static const unsigned char songData[SONG_DATA_LEN] = {\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            f.write("  " + ",".join(str(b) for b in chunk) + ",\n")
        f.write("};\n")
    print(f"{out_path}: {len(data)} Bytes eingebettet.")


if __name__ == "__main__":
    main()
