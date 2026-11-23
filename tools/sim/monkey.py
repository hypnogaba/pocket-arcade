#!/usr/bin/env python3
"""Write a long random script for tools/sim/host.ts: keys, chords held for
the menu, ticks at 16 and 30 ms, relaunches. Seeded, so a failure repeats.
  monkey.py <seed> <steps> > build/monkey.json
"""
import json, random, sys
r = random.Random(int(sys.argv[1]))
steps = []
for _ in range(int(sys.argv[2])):
    x = r.random()
    if x < 0.35:
        steps.append({"key": r.choice(["up", "down", "down", "select", "select", "back"])})
    elif x < 0.55:
        steps.append({"tick": r.randint(1, 60), "ms": r.choice([16, 30])})
    elif x < 0.75:
        steps.append({"tick": r.randint(1, 40), "ms": 16, "hold": r.choice(["right", "left", "up", "down", "select", "back", "right,select", "up,back"])})
    elif x < 0.85:
        steps.append({"tick": r.choice([2, 3, 70, 80]), "ms": 16, "hold": "select,back"})
    elif x < 0.97:
        steps.append({"select_row": r.choice(["Save state", "Load state", "Colours", "Screen", "Restart", "Save and go", "Credits", "How to play", "Continue", "Broken", "Fake", "Snake", "uCity", "Aevilia", "Libbet", "Back to the game", "Press SELECT"])})
    else:
        steps.append({"exit": True})
print(json.dumps(steps))
