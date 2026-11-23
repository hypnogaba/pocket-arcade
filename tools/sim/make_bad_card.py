#!/usr/bin/env python3
"""Put broken games on a test card: random bytes, a cartridge type the core
doesn't know (with a valid header checksum), and a file too small to list."""
import os, random, sys
d = sys.argv[1]
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
r = random.Random(7)
open(os.path.join(d, 'Broken.gb'), 'wb').write(bytes(r.randrange(256) for _ in range(65536)))
b = bytearray(open(os.path.join(root, 'roms', 'libbet.gb'), 'rb').read())
b[0x147] = 0xEE
x = 0
for i in range(0x134, 0x14D):
    x = (x - b[i] - 1) & 0xFF
b[0x14D] = x
open(os.path.join(d, 'Fake.gbc'), 'wb').write(bytes(b))
open(os.path.join(d, 'Tiny.gb'), 'wb').write(b'x' * 100)
