#!/bin/bash
# Build the app with the store's toolchain and run every simulator script.
# bash 3.2 (macOS): no associative arrays.
set -u
cd "$(dirname "$0")/../.."
SDK=${SDK:-$HOME/Projects/solana-os-sdk}
"$SDK/tools/cc/badge-cc" app -o build/badge/app.wasm --aot build/badge/app.aot -q || exit 1
SIM="node --experimental-strip-types tools/sim/host.ts build/badge/app.wasm --app app --storage build/storage.json --shots build/shots --quiet"
fail=0
fresh() { rm -rf build/card build/storage.json; mkdir -p build/card; }
run() { # name script [extra card setup already done]
    out=$($SIM --card build/card --script "$2" 2>&1)
    if echo "$out" | grep -q "PROBLEM"; then
        echo "FAIL $1"; echo "$out" | grep PROBLEM | head -5; fail=1
    else
        echo "ok   $1: $(echo "$out" | grep '^sim:')"
    fi
}
fresh; cp roms/2048.gb "build/card/My 2048.gb"; run flow tools/sim/flow.json
fresh; python3 tools/sim/make_bad_card.py build/card; run broken tools/sim/broken.json
fresh; run secrets tools/sim/secrets.json
for seed in 1 2 3 4 5; do
    fresh; python3 tools/sim/make_bad_card.py build/card; cp roms/2048.gb "build/card/My 2048.gb"
    python3 tools/sim/monkey.py $seed 1500 > build/monkey.json
    out=$($SIM --card build/card --script build/monkey.json 2>&1 | grep -v 'select_row: no row\|no list on screen')
    if echo "$out" | grep -q PROBLEM; then echo "FAIL monkey $seed"; echo "$out" | grep PROBLEM | head -5; fail=1
    else echo "ok   monkey $seed: $(echo "$out" | grep '^sim:')"; fi
done
exit $fail
