#!/usr/bin/env python3
"""Make app/walnut/ from vendor/walnut/ (tools/strip_c.py) and prove the
stripped core is the same machine: build the harness twice, once on each,
run every bundled game and compare a CRC of every frame drawn.
"""
import json, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, 'build')

def sh(*a):
    subprocess.run(a, check=True)

def make_stripped():
    dst = os.path.join(ROOT, 'app', 'walnut')
    os.makedirs(dst, exist_ok=True)
    notice = os.path.join(ROOT, 'vendor', 'walnut', 'NOTICE.txt')
    for f in ('walnut_cgb.h', 'walnut_cgb_cpu.h'):
        sh(sys.executable, os.path.join(ROOT, 'tools', 'strip_c.py'),
           os.path.join(ROOT, 'vendor', 'walnut', f), os.path.join(dst, f),
           *( [notice] if f == 'walnut_cgb.h' else [] ))
    shutil.copy(os.path.join(ROOT, 'vendor', 'walnut', 'license.txt'), os.path.join(dst, 'license.txt'))

def build(name, walnut_dir):
    d = os.path.join(B, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    shutil.copy(os.path.join(ROOT, 'app', 'emu.c'), d)
    shutil.copy(os.path.join(ROOT, 'app', 'emu.h'), d)
    os.symlink(walnut_dir, os.path.join(d, 'walnut'))
    exe = os.path.join(d, 'harness')
    sh('clang', '-O2', '-std=gnu11', '-w', '-o', exe, os.path.join(ROOT, 'tools', 'harness.c'), os.path.join(d, 'emu.c'))
    return exe

def main():
    make_stripped()
    a = build('core-vendor', os.path.join(ROOT, 'vendor', 'walnut'))
    b = build('core-app', os.path.join(ROOT, 'app', 'walnut'))
    meta = json.load(open(os.path.join(ROOT, 'tools', 'games.json')))
    bad = 0
    tmp = os.path.join(B, 'core-shots')
    os.makedirs(tmp, exist_ok=True)
    for g in meta['games']:
        rom = os.path.join(ROOT, 'roms', g['rom'])
        ra = json.loads(subprocess.run([a, rom, tmp, '2400'], capture_output=True, text=True).stdout.splitlines()[-1])
        rb = json.loads(subprocess.run([b, rom, tmp, '2400'], capture_output=True, text=True).stdout.splitlines()[-1])
        same = ra.get('frames_crc') == rb.get('frames_crc') and 'error' not in ra
        bad += not same
        print(f"{g['id']:12} {ra.get('frames_crc')} {rb.get('frames_crc')} {'same' if same else 'DIFFERENT'}")
    print('core check:', 'ok' if not bad else f'{bad} differ')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
