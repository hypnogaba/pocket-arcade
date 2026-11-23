#!/usr/bin/env python3
"""Run every ROM in a folder through the harness: does it boot, does the
picture move, how heavy is it for the badge. Writes a contact sheet per
game (build/survey/<name>.png) and a table (build/survey/survey.json).

Badge speed is estimated from Libbet, measured on a production badge in
docs/C_API.md: 4.9 ms CPU a frame native, 10.5 ms more to draw a frame.
"""
import json, os, subprocess, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(ROOT, 'build', 'harness')

def run(rom, out, frames=3600, script='', palette=0):
    os.makedirs(out, exist_ok=True)
    p = subprocess.run([HARNESS, rom, out, str(frames), script, str(palette)],
                       capture_output=True, text=True, timeout=300)
    line = (p.stdout.strip().splitlines() or ['{}'])[-1]
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        return {'error': 'bad output: ' + line[:80]}

def sheet(shots, dst):
    ims = [Image.open(s).convert('RGB') for s in shots]
    if not ims:
        return
    w = Image.new('RGB', (160 * len(ims) + 4 * (len(ims) - 1), 144), (40, 40, 40))
    for i, im in enumerate(ims):
        w.paste(im, (i * 164, 0))
    w.save(dst)

def badge_estimate(r, ref):
    k_cpu = 4.9 / ref['cpu_us']
    k_draw = 10.5 / max(1e-3, ref['draw_us'] - ref['cpu_us'])
    cpu = r['cpu_us'] * k_cpu
    draw = max(0.0, r['draw_us'] - r['cpu_us']) * k_draw + 1.2   # + ui_frame, no copy
    # The app emulates up to 3 frames a tick and draws the last: emulated fps
    # is limited by cpu + draw/frames-per-tick.
    best = 0
    for per_tick in (1, 2, 3):
        t = cpu * per_tick + draw
        fps = min(59.7, per_tick * 1000.0 / t)
        best = max(best, fps)
    return round(cpu, 1), round(draw, 1), round(best, 1)

def main():
    romdir = sys.argv[1]
    out = os.path.join(ROOT, 'build', 'survey')
    os.makedirs(out, exist_ok=True)
    ref = run(os.path.join(romdir, 'libbet.gb'), os.path.join(out, '_ref'), 2000)
    rows = []
    for name in sorted(os.listdir(romdir)):
        if not name.lower().endswith(('.gb', '.gbc', '.cgb')):
            continue
        slug = os.path.splitext(name)[0]
        r = run(os.path.join(romdir, name), os.path.join(out, slug))
        r['slug'] = slug
        if 'error' not in r:
            r['badge_cpu_ms'], r['badge_draw_ms'], r['badge_fps'] = badge_estimate(r, ref)
            sheet(r['shots'], os.path.join(out, slug + '.png'))
        rows.append(r)
        print(f"{slug:32} {r.get('error') or ''} cgb={r.get('cgb')} cpu={r.get('badge_cpu_ms')}ms "
              f"fps~{r.get('badge_fps')} moved={r.get('changed')}/{r.get('drawn')} lit={r.get('lit')}")
    json.dump(rows, open(os.path.join(out, 'survey.json'), 'w'), indent=1)

if __name__ == '__main__':
    main()
