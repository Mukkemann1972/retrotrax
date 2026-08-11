#!/usr/bin/env python3
"""Baut eine bootfaehige leere Amiga-ADF-Diskette aus einem rohen 68k-Binaerblob.

Bootblock-Layout (Amiga Floppy Boot Process, devices/bootblock.h):
  Bytes 0-3:  ID "DOS" + Flags (0x00 = OFS, kein Filesystem noetig fuer uns)
  Bytes 4-7:  Checksumme (additive Ones'-Complement-Summe aller 256 Longwords
              inkl. dieses Feldes muss 0xFFFFFFFF ergeben)
  Bytes 8-11: BPTR auf Root-Block (nicht relevant ohne Filesystem, 0)
  Bytes 12+:  Code, wird vom ROM-Bootstrap direkt angesprungen (kein rts noetig)
"""
import sys
import struct

ADF_SIZE = 901120  # 80 Tracks * 2 Seiten * 11 Sektoren * 512 Bytes
BOOTBLOCK_SIZE = 1024
CODE_MAX = BOOTBLOCK_SIZE - 12


def build_bootblock(code: bytes) -> bytes:
    if len(code) > CODE_MAX:
        raise ValueError(f"Code zu gross fuer Bootblock: {len(code)} > {CODE_MAX} Bytes")
    block = bytearray(BOOTBLOCK_SIZE)
    block[0:4] = b"DOS\x00"
    block[8:12] = struct.pack(">I", 0)
    block[12:12 + len(code)] = code

    # Checksumme: ones'-complement end-around-carry Summe aller 256 Longwords
    # (Checksummenfeld dabei auf 0), Ergebnis-Checksumme = Komplement der Summe.
    total = 0
    for i in range(0, BOOTBLOCK_SIZE, 4):
        if i == 4:
            continue  # Checksummenfeld selbst zaehlt als 0
        word = struct.unpack(">I", block[i:i + 4])[0]
        total += word
        total = (total & 0xFFFFFFFF) + (total >> 32)
    checksum = (~total) & 0xFFFFFFFF
    block[4:8] = struct.pack(">I", checksum)
    return bytes(block)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <code.bin> <out.adf>")
        sys.exit(1)
    code_path, adf_path = sys.argv[1], sys.argv[2]
    with open(code_path, "rb") as f:
        code = f.read()
    bootblock = build_bootblock(code)
    image = bootblock + bytes(ADF_SIZE - BOOTBLOCK_SIZE)
    with open(adf_path, "wb") as f:
        f.write(image)
    print(f"{adf_path}: {len(image)} Bytes, Code {len(code)}/{CODE_MAX} Bytes,"
          f" Checksumme 0x{struct.unpack('>I', bootblock[4:8])[0]:08X}")


if __name__ == "__main__":
    main()
