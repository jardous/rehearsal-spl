# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location(
    "make_uf2", Path(__file__).resolve().parents[1] / "tools/make_uf2.py")
uf2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uf2)


def image(payload=bytes(range(256)) * 3, hashed=True):
    header = bytearray(24)
    header[0:2] = bytes((0xE9, 1))
    struct.pack_into("<H", header, 12, 9)
    header[23] = int(hashed)
    result = header + struct.pack("<II", 0x3FC90000, len(payload)) + payload
    checksum = 0xEF
    for byte in payload:
        checksum ^= byte
    result += bytes((len(result) | 15) - len(result)) + bytes((checksum,))
    if hashed:
        result += hashlib.sha256(result).digest()
    return bytes(result)


class UF2Tests(unittest.TestCase):
    def test_roundtrip(self):
        for hashed in (False, True):
            for size in (1, 16, 255, 256, 257, 768):
                data = image(bytes(i % 256 for i in range(size)), hashed)
                result = uf2.convert(data)
                count = (len(data) + 255) // 256
                self.assertEqual(len(result), count * 512)
                decoded = bytearray()
                for number in range(count):
                    block = result[number * 512:(number + 1) * 512]
                    self.assertEqual(struct.unpack("<8I", block[:32]),
                        (0x0A324655, 0x9E5D5157, 0x2000, number * 256,
                         256, number, count, 0xC47E5767))
                    self.assertEqual(struct.unpack("<I", block[508:])[0], 0x0AB16F30)
                    decoded += block[32:288]
                self.assertEqual(decoded[:len(data)], data)

    def test_wrong_chip(self):
        data = bytearray(image())
        data[12] = 2
        with self.assertRaises(ValueError):
            uf2.convert(data)

    def test_corruption_and_truncation(self):
        good = image()
        for length in (0, 23, 24, 31, 32, len(good) - 1):
            with self.assertRaises(ValueError):
                uf2.convert(good[:length])
        for offset in (0, 1, 23, 32, len(good) - 33, len(good) - 1):
            data = bytearray(good)
            data[offset] ^= 0x80
            with self.assertRaises(ValueError):
                uf2.convert(data)

    def test_oversized(self):
        with self.assertRaises(ValueError):
            uf2.convert(bytes(uf2.APP_SIZE + 1))


if __name__ == "__main__":
    unittest.main()
