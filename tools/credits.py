#!/usr/bin/env python3
"""Write CREDITS.md from tools/games.json: every bundled game, who made it,
its licence and where its source is."""
import json, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
meta = json.load(open(os.path.join(ROOT, 'tools', 'games.json')))
out = ['# Credits', '',
       'Pocket Arcade bundles these free games, unchanged, under their own licences.',
       'Each was checked in its own repository on 2026-11-23. The ROM files come from',
       '[Homebrew Hub](https://hh.gbdev.io) (github.com/gbdev/database); `tools/games.json`',
       'pins each file by SHA-256. GPL-licensed games: their source is at the link given.', '',
       '| Game | Genre | Made by | Licence | Source |', '|---|---|---|---|---|']
for g in meta['games']:
    out.append(f"| {g['title']} | {g['genre']} | {g['author']} | {g['license']} | [{g['source']}](https://{g['source']}) |")
out += ['', '## Code inside the app', '',
        '| Part | Made by | Licence |', '|---|---|---|',
        '| Walnut-CGB 1.2.4 (Game Boy and Color core), from the Solana OS C SDK | Mr-PauI; Peanut-GB by Mahyar Koshkouei; parts from SameBoy by Lior Halphon | MIT |',
        '| tinf (inflate) | Joergen Ibsen | zlib |',
        '| Pocket Arcade (main.c, emu.c, pack.c, tools) | @hypnogaba | MIT |', '',
        'Game Boy and Game Boy Color are trademarks of Nintendo. Pocket Arcade is not made,',
        'licensed or endorsed by Nintendo.', '']
s = '\n'.join(out)
assert chr(0x2014) not in s
open(os.path.join(ROOT, 'CREDITS.md'), 'w').write(s)
print('CREDITS.md', len(meta['games']), 'games')
