#!/bin/sh
#
# roundtrip-sizes.sh - pack/unpack must be a fixed point at any corpus size.
#
# Small corpora are where the compressor's edge cases live: tables that end up
# with a single distinct symbol, and symbols that pass 2 emits but pass 1 never
# counted. Uses a slice of a real segment when the ROMs are available.

set -e
cd "$(dirname "$0")/.."

KIPACK=./kipack
# Same name and same default as check.sh, which passes its own value through -
# otherwise ROMS=<dir> ./check.sh would run this sweep against a different ROM
# set, find nothing, skip, and still be counted as a pass.
ROMS=${ROMS:-../../assets/roms}
SIZES="4 16 64 256 1024 4096"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if [ ! -f "$ROMS/ki-l15d.u98" ]; then
	echo "roundtrip-sizes: $ROMS/ki-l15d.u98 not present - skipping"
	exit 0
fi

mkdir -p "$WORK/full"
"$KIPACK" unpack "$ROMS/ki-l15d.u98" "$WORK/full" > /dev/null

pass=0
fail=0
for n in $SIZES; do
	d="$WORK/n$n"
	mkdir -p "$d/in" "$d/out"
	dd if="$WORK/full/rom-0.bin" of="$d/in/rom-0.bin" bs=4 count="$n" 2> /dev/null
	cp "$WORK/full/rom-0.addr" "$d/in/rom-0.addr"

	if ! "$KIPACK" pack "$d/in" "$d/t.packed" > "$d/pack.log" 2>&1; then
		printf '  %-10s PACK FAILED: %s\n' "n=$n" "$(grep -i 'pack:' "$d/pack.log" | head -1)"
		fail=$((fail + 1)); continue
	fi
	if ! "$KIPACK" unpack -t packed "$d/t.packed" "$d/out" > "$d/unpack.log" 2>&1; then
		printf '  %-10s UNPACK FAILED\n' "n=$n"
		fail=$((fail + 1)); continue
	fi
	if cmp -s "$d/in/rom-0.bin" "$d/out/rom-0.bin"; then
		printf '  %-10s OK\n' "n=$n"; pass=$((pass + 1))
	else
		printf '  %-10s ROUND-TRIP MISMATCH\n' "n=$n"; fail=$((fail + 1))
	fi
done

echo "$pass passed, $fail failed"
[ "$fail" = "0" ]
