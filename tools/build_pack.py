#!/usr/bin/env python3
"""Build the bundled games for Pocket Arcade: app/games/index.bin and
app/games/p<N>.bin from tools/games.json, the ROMs in roms/ and the
harness screenshots in build/survey/<slug>/.

Layout (all little-endian), read by app/pack.c:

index.bin   "PAI1", u16 count, u16 version, then count records of 256 bytes:
              0 id[12]  12 title[24]  36 genre[12]  48 author[40]
             88 license[24]  112 source[48]  160 blurb[72]
            232 u8 pack, u8 flags (1: Color game, 2: written for the badge,
                no ROM and banks 0), u8 palette, u8 0
            236 u16 banks, u16 0   240 u32 blob offset in the pack
            244 u32 preview offset  248 u32 preview length  252 u32 crc32
p<N>.bin    the games' blobs, each: (banks + 1) u32 offsets from the blob's
            start, then each 16 KB bank as raw deflate (or stored as is
            when that isn't smaller: its length is then 16384).
            A preview: u16 colours, colours x u16 big-endian RGB565, then
            raw deflate of 160 x 144 one-byte colour indexes.

Every pack stays under the store's 256 KB a file; the folder under 1 MB.
"""
import json, os, struct, sys, zlib
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BANK = 16384
PACK_MAX = 256 * 1024 - 1
REC = 256

def deflate(b):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return c.compress(b) + c.flush()

def fixed(s, n):
    b = s.encode('ascii')
    if len(b) >= n:
        raise SystemExit(f'too long for {n} bytes: {s!r}')
    return b + b'\0' * (n - len(b))

def rgb565_be(r, g, b):
    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return struct.pack('>H', v)

def preview(path):
    im = Image.open(path).convert('RGB')
    assert im.size == (160, 144), path
    colours = sorted(set(im.getdata()))
    if len(colours) > 256:
        im = im.quantize(256).convert('RGB')
        colours = sorted(set(im.getdata()))
    idx = {c: i for i, c in enumerate(colours)}
    px = bytes(idx[c] for c in im.getdata())
    out = struct.pack('<H', len(colours)) + b''.join(rgb565_be(*c) for c in colours) + deflate(px)
    return out

def wrap_lines(text, width=22):
    """The lines app/main.c's wrap() makes of a blurb (the list shows 3)."""
    lines, p = [], text
    while p:
        p = p.lstrip(' ')
        if not p:
            break
        take = min(len(p), width)
        if len(p) > width:
            cut = take
            while cut > 0 and p[cut] != ' ':
                cut -= 1
            if cut > 0:
                take = cut
        lines.append(p[:take])
        p = p[take:]
    return lines

def rom_blob(rom):
    size = 0x8000
    while size < len(rom):
        size *= 2
    rom = rom + b'\xff' * (size - len(rom))
    banks = size // BANK
    data = []
    for i in range(banks):
        raw = rom[i * BANK:(i + 1) * BANK]
        z = deflate(raw)
        data.append(z if len(z) < BANK else raw)
    head = 4 * (banks + 1)
    offs, pos = [], head
    for d in data:
        offs.append(pos)
        pos += len(d)
    offs.append(pos)
    blob = struct.pack(f'<{banks + 1}I', *offs) + b''.join(data)
    return blob, banks, zlib.crc32(rom) & 0xFFFFFFFF, rom

def main():
    meta = json.load(open(os.path.join(ROOT, 'tools', 'games.json')))
    argv = sys.argv[1:]
    out = os.path.join(ROOT, 'app', 'games')
    if '--out' in argv:                      # a test build elsewhere, e.g. a few games for the emulator
        out = argv[argv.index('--out') + 1]
    if '--only' in argv:
        keep = argv[argv.index('--only') + 1].split(',')
        meta['games'] = [g for g in meta['games'] if g['id'] in keep]
    romdir = os.path.join(ROOT, 'roms')
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        os.remove(os.path.join(out, f))
    packs = [bytearray()]
    recs = []
    for g in meta['games']:
        if len(wrap_lines(g['blurb'])) > 3:
            raise SystemExit(f"{g['id']}: the blurb needs more than the list's 3 lines: {wrap_lines(g['blurb'])}")
        if g.get('native'):
            # written for the badge (app/tanks.c): no ROM, only the preview;
            # the crc names its snapshots
            blob, banks, crc = b'', 0, zlib.crc32(b'native:' + g['id'].encode()) & 0xFFFFFFFF
            prev = preview(os.path.join(ROOT, g['preview']))
            flags = 2
            rom = b''
        else:
            rom = open(os.path.join(romdir, g['rom']), 'rb').read()
            blob, banks, crc, padded = rom_blob(rom)
            shot = os.path.join(ROOT, 'build', 'survey', g['slug'], 'f%05d.ppm' % g['shot'])
            prev = preview(shot)
            flags = 1 if padded[0x143] in (0x80, 0xC0) else 0
        need = len(blob) + len(prev)
        if need > PACK_MAX:
            raise SystemExit(f"{g['id']} is {need} bytes packed: more than one file can hold")
        if len(packs[-1]) + need > PACK_MAX:
            packs.append(bytearray())
        p = packs[-1]
        off = len(p)
        p += blob
        poff = len(p)
        p += prev
        rec = (fixed(g['id'], 12) + fixed(g['title'], 24) + fixed(g['genre'], 12) + fixed(g['author'], 40) +
               fixed(g['license'], 24) + fixed(g['source'], 48) + fixed(g['blurb'], 72) +
               struct.pack('<BBBBHHIIII', len(packs) - 1, flags, g.get('palette', 0), 0, banks, 0,
                           off, poff, len(prev), crc))
        assert len(rec) == REC
        recs.append(rec)
        print(f"{g['id']:12} {len(rom) // 1024:5} KB -> {len(blob) // 1024:4} KB + preview {len(prev) // 1024} KB  pack {len(packs) - 1}")
    index = b'PAI1' + struct.pack('<HH', len(recs), 1) + b''.join(recs)
    open(os.path.join(out, 'index.bin'), 'wb').write(index)
    total = len(index)
    for i, p in enumerate(packs):
        open(os.path.join(out, f'p{i}.bin'), 'wb').write(p)
        total += len(p)
    print(f'{len(recs)} games, {len(packs)} packs, {total} bytes ({total / 1024:.0f} KB)')

if __name__ == '__main__':
    main()
