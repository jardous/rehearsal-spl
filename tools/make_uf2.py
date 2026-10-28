#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package an ESP32-S3 *application only* for TinyUF2 (partition-relative base 0)."""
import argparse
import hashlib
from pathlib import Path
import struct

FAMILY = 0xC47E5767
APP_SIZE = 0x200000


def validate_image(image):
    if not 24 <= len(image) <= APP_SIZE:
        raise ValueError("Application image is truncated or exceeds the 2 MiB OTA partition")
    if image[0] != 0xE9 or not 1 <= image[1] <= 16:
        raise ValueError("Expected an ESP application .bin, not a merged flash image")
    if struct.unpack_from("<H", image, 12)[0] != 9:
        raise ValueError("Image is not for ESP32-S3")
    if image[23] not in (0, 1):
        raise ValueError("Unsupported image hash flag")
    pos, checksum = 24, 0xEF
    for _ in range(image[1]):
        if pos + 8 > len(image):
            raise ValueError("Truncated segment header")
        length = struct.unpack_from("<I", image, pos + 4)[0]
        pos += 8
        if pos + length > len(image):
            raise ValueError("Truncated segment data")
        for byte in image[pos:pos + length]:
            checksum ^= byte
        pos += length
    checksum_at = pos | 15
    end = checksum_at + 1
    if len(image) != end + (32 if image[23] else 0):
        raise ValueError("Unexpected application length or unsupported signed image")
    if image[checksum_at] != checksum:
        raise ValueError("Invalid application checksum")
    if image[23] and hashlib.sha256(image[:end]).digest() != image[end:]:
        raise ValueError("Invalid application SHA-256")


def convert(image):
    validate_image(image)
    count = (len(image) + 255) // 256
    blocks = []
    for number in range(count):
        header = struct.pack("<8I", 0x0A324655, 0x9E5D5157, 0x2000,
                             number * 256, 256, number, count, FAMILY)
        payload = image[number * 256:(number + 1) * 256].ljust(256, b"\x00")
        blocks.append(header + payload + bytes(220) + struct.pack("<I", 0x0AB16F30))
    return b"".join(blocks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("Input and output must be different files")
    try:
        data = convert(args.input.read_bytes())
    except ValueError as error:
        parser.error(str(error))
    args.output.write_bytes(data)
    print(f"UF2: {args.output} ({len(data)} bytes; ESP32-S3, app-only, base 0)")
    print(f"SHA256: {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
