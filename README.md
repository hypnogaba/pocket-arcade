# Pocket Arcade

A Game Boy and Game Boy Color player for the Solana BP26 badge, with 16 free
games inside, an original tank game written for the badge, and room for your
own on the microSD card.

- **Tank Siege**, made for Pocket Arcade: guard your base from 20 enemy tanks
  a stage. Bricks break, steel holds, water stops tanks, trees hide them.
  14 stages drawn by hand, then they come round again, harder. Flashing tanks
  drop bonuses: a star (faster, then double, then steel-breaking shots), a
  helmet, a clock, a shovel (steel round the base), a grenade, a tank.
- **16 games, one install.** Platformers, shooters, puzzles, a city builder,
  a roguelike, racing. All free software or freely licensed: see
  [CREDITS.md](CREDITS.md).
- **A preview of every game** in the list, taken from the game itself.
- **Your own games.** Copy `.gb` or `.gbc` files you own to
  `hh/files/pocket-arcade/` on the card. Games up to 2 MB load whole, bigger
  ones page from the card.
- **Never lose a game.** HOME keeps the game you were playing: it is first in
  the list next time. Battery saves go to the card, and the menu has a save
  state for each game.
- **Colours** for original Game Boy games (six palettes, one per layer, kept
  per game), **1x or 1.5x** screen, **A/B swap**, and the **LEDs glow** with
  the top of the picture.

The badge has no speaker, so the games are silent.

## 0.3.0 (23 Nov 2026)

- **Starts on the list**, with Secrets, Leaderboard, Share on X, Settings, How to play, About
  (a QR to [@hypnogaba](https://x.com/hypnogaba)) and Credits as rows.
- **START and exit said up front**: every game starts with a toast (tap SELECT+CANCEL = START,
  hold both 1 s = menu and exit); How to play leads with it; the menu has "Press START".
- **Full screen (1.5x) by default.** The Game Boy picture is 10:9, so 1.5x fills the height.
- **Every game its own colours** instead of all green (the menu still changes them).
- **5 secrets** with hints on where to look (Konami on the list, save + load a state, 5 games,
  a flawless Tank Siege stage, 3 stars). Each opens a colour set (Gold, Neon, Mint, Ruby,
  DoubleZero); all five give a Solana-purple tank.
- **Tank Siege leaderboard online** and **share on X** with a picture card (server/: the
  `pocket-arcade` worker, D1 `pocket-arcade-scores`, signed badge requests).
- Swapped the three heaviest colour games (Snake, Dusky Dungeon, GBHack) for light original
  Game Boy ones (Wyrmhole, PostBot, Squishy the Turtle): simple 8-bit pictures, less work a frame.

## Keys

| Badge | Game Boy |
|---|---|
| D-pad | D-pad |
| SELECT | A |
| CANCEL | B |
| SELECT + CANCEL, tap | START (or the menu's Press START) |
| SELECT + CANCEL, hold 1 s | the menu: SELECT button, save and load state, colours, screen, LEDs, A/B swap, restart, back to the list |
| HOME | leave (the game is kept) |

## How it works

| File | What |
|---|---|
| `app/main.c` | The badge side: the list, loading, frames, keys, menu, saves, LEDs |
| `app/emu.c` | Walnut-CGB behind a small API with no badge calls: palettes, snapshots, the joypad interrupt; a native game behind the same API |
| `app/tanks.c` | Tank Siege: draws a 160 x 144 picture like a Game Boy game, so the app scales, saves and snapshots it the same way. Its whole state is one struct with no pointers, so a snapshot is a copy |
| `app/pack.c` | The bundled games: an index, then each game's 16 KB banks deflated one by one, and a preview |
| `app/walnut/` | The core, comments stripped to fit the store's 1 MB (made from `vendor/walnut/`) |
| `vendor/walnut/` | Walnut-CGB 1.2.4 from the Solana OS C SDK, with one fix marked "Pocket Arcade" (below) |
| `app/games/` | The packed games, made by `tools/build_pack.py` |

The store compiles C apps to the badge's native code; only that runs games at
full speed (interpreted, in the emulator or "Run on badge", it is under 1 fps).
Measured on the badge for the SDK's example with the same core: 53 to 60
emulated frames a second for most homebrew. The pacing emulates up to three
frames a tick and shows the last, so a heavy game keeps its speed and shows
fewer frames.

**The core fix.** Walnut-CGB's HALT loop never ends when a game halts with the
screen off and waits for a button (it doesn't raise the joypad interrupt):
on the badge that is "App took too long". `vendor/walnut/walnut_cgb_cpu.h`
now ends a HALT at the end of a frame and resumes it on the next one, and
`emu.c` raises the joypad interrupt when a button goes down, as the console
does. Snake hung before the fix and plays after it.

## Build and test

Needs the Solana OS C SDK toolchain (`sh tools/cc/fetch.sh` in a checkout of
github.com/solana-foundation/solana-os, branch `release-2.3`, at
`~/Projects/solana-os-sdk`), Python 3 with Pillow, clang, and Node 22.

```sh
python3 tools/fetch_roms.py          # the games, checked against games.json
python3 tools/build_pack.py          # app/games/
python3 tools/check_core.py          # app/walnut/ from vendor/, same frames in every game
python3 tools/credits.py             # CREDITS.md
cc -O1 -fsanitize=address,undefined -Iapp tools/tank_test.c app/tanks.c -o build/tank_test
build/tank_test build/tanks          # Tank Siege's tests, and the preview picture
tools/sim/run_all.sh                 # build with the store's toolchain, run the simulator scripts
```

| Test | What it proves |
|---|---|
| `tools/harness.c` + `tools/survey.py` | Every game boots in the core, its picture moves, an estimate of its speed on the badge |
| `tools/test_pack.c` | Every game unpacks to its exact ROM and CRC, previews decode, a snapshot loads back to the same next frame, a damaged pack is refused |
| `tools/check_core.py` | The stripped core draws the same frames as the original in every bundled game |
| `tools/tank_test.c` | Tank Siege on a computer with sanitizers: every stage lets the enemies reach the base and the player reach the top; random games with the whole state checked every frame; 32 stages cleared in a row with a cheat; a defending bot wins part of its games on the first stages (the enemies were tuned with it); a snapshot plays on frame for frame the same |
| `tools/sim/host.ts` | The whole app (the store's `app.wasm`) against a simulated badge: real files, saves, relaunch after HOME, a scripted flow, broken card files, and 5 random runs of 1,500 steps each |

`tools/sim/host.ts` is made from the SDK's `sdk/c/tests/mock-host.ts` (MIT,
Solana Foundation).

## Licence

The code of Pocket Arcade is MIT ([LICENSE](LICENSE)). The games, the core and
the inflate library keep their own licences ([CREDITS.md](CREDITS.md)).
Game Boy is a trademark of Nintendo; this app is not made or endorsed by
Nintendo.
