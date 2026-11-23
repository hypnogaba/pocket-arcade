#!/usr/bin/env python3
"""Download the bundled games from Homebrew Hub (github.com/gbdev/database)
into roms/, by the slug and file in tools/games.json, and check each
against its sha256 there. With --pin, write the hashes of what was
downloaded into games.json instead (first time, or after an update).
"""
import hashlib, json, os, sys, urllib.parse, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 'https://raw.githubusercontent.com/gbdev/database/master/entries/'

def main():
    pin = '--pin' in sys.argv
    path = os.path.join(ROOT, 'tools', 'games.json')
    meta = json.load(open(path))
    os.makedirs(os.path.join(ROOT, 'roms'), exist_ok=True)
    bad = 0
    for g in meta['games']:
        if g.get('native'):
            continue                    # written for the badge: no ROM
        dst = os.path.join(ROOT, 'roms', g['rom'])
        if not os.path.exists(dst):
            entry = json.load(urllib.request.urlopen(BASE + g['slug'] + '/game.json'))
            files = [f for f in entry['files'] if isinstance(f, dict)]
            f = next((f for f in files if f.get('default')), files[0])['filename']
            urllib.request.urlretrieve(BASE + g['slug'] + '/' + urllib.parse.quote(f), dst)
        h = hashlib.sha256(open(dst, 'rb').read()).hexdigest()
        if pin:
            g['sha256'] = h
        elif g.get('sha256') != h:
            print('HASH MISMATCH', g['rom'], h)
            bad += 1
    if pin:
        open(path, 'w').write(json.dumps(meta, indent=1, ensure_ascii=False) + '\n')
    print('ok' if not bad else f'{bad} bad')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
