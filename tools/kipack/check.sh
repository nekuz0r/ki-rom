#!/bin/sh
#
# check.sh - regression harness for the kipack applets.
#
# Verifies, when the ROMs are available:
#   - unpack output still matches the recorded baseline
#   - pack output still matches the recorded baseline
#   - unpack -> pack -> unpack is a fixed point
#
# Usage: ./check.sh [--update]
#   --update regenerates the baseline manifests instead of checking them.

set -e
cd "$(dirname "$0")"

ROMS=${ROMS:-../../assets/roms}
KIPACK=./kipack
EXPECTED=fixtures/expected

UPDATE=0
[ "$1" = "--update" ] && UPDATE=1

sha() {
	if command -v sha256sum > /dev/null 2>&1; then sha256sum "$@"; else shasum -a 256 "$@"; fi
}

pass=0
fail=0
ok()  { pass=$((pass + 1)); printf '  %-28s OK\n' "$1"; }
bad() { fail=$((fail + 1)); printf '  %-28s FAILED\n' "$1"; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if [ ! -x "$KIPACK" ]; then
	echo "check: $KIPACK not built - run make first" >&2
	exit 1
fi

# Run a command under a wall-clock bound and return its exit status.
#
# The malformed-input cases below all used to hang rather than fail, so an
# unbounded run would wedge the suite instead of reporting. `timeout` is not
# POSIX and is absent from a stock macOS, hence doing it by hand. A run that
# gets killed reports its signal status, which is not the 1 the callers require
# - so a regression to hanging shows up as a failure, not a pass.
run_bounded() {
	"$@" > /dev/null 2>&1 &
	bounded_pid=$!
	( sleep 15; kill -9 "$bounded_pid" ) 2> /dev/null &
	bounded_killer=$!
	wait "$bounded_pid" 2> /dev/null
	bounded_status=$?
	kill "$bounded_killer" 2> /dev/null
	wait "$bounded_killer" 2> /dev/null
	return $bounded_status
}

# Overwrite one byte of a file in place. offset is decimal, value octal.
poke() {
	printf "$3" | dd of="$1" bs=1 seek="$2" conv=notrunc 2> /dev/null
}

# Require unpack to reject an archive with its own exit code 1, promptly.
reject_case() {
	reject_name=$1
	reject_file=$2

	rm -rf "$WORK/syn/rej"
	mkdir -p "$WORK/syn/rej"
	# Captured through `if`, not a bare call: `set -e` is in effect, and would
	# abort the suite on the very non-zero status this is here to inspect.
	if run_bounded "$KIPACK" unpack -t packed "$reject_file" "$WORK/syn/rej"; then
		reject_status=0
	else
		reject_status=$?
	fi
	if [ "$reject_status" = "1" ]; then
		ok "reject $reject_name"
	else
		bad "reject $reject_name (exit $reject_status, wanted 1)"
	fi
}

# Copy an image to a name that should sniff to a given variant, unpack it with
# no -t, and require the result to match the corpus. Not marked `local`: this
# is #!/bin/sh and `local` is not POSIX.
sniff_case() {
	sniff_name=$1
	sniff_src=$2

	cp "$sniff_src" "$WORK/syn/$sniff_name"
	mkdir -p "$WORK/syn/sniff-$sniff_name"
	if "$KIPACK" unpack "$WORK/syn/$sniff_name" "$WORK/syn/sniff-$sniff_name" \
			> /dev/null 2>&1 &&
		diff -r -q "$WORK/syn/in" "$WORK/syn/sniff-$sniff_name" > /dev/null 2>&1
	then
		ok "sniff $sniff_name"
	else
		bad "sniff $sniff_name"
	fi
}

# ---------------------------------------------------------- synthetic checks
# These use no ROM data, so they always run.
check_synthetic() {
	echo "synthetic:"

	if [ ! -x fixtures/mkcorpus ] || [ ! -x fixtures/mkromimage ]; then
		echo "  fixture tools not built - run make check" >&2
		fail=$((fail + 1))
		return
	fi

	mkdir -p "$WORK/syn/in" "$WORK/syn/out"
	fixtures/mkcorpus "$WORK/syn/in"
	"$KIPACK" pack "$WORK/syn/in" "$WORK/syn/corpus.packed" > /dev/null

	# 1. packed round-trip
	"$KIPACK" unpack -t packed "$WORK/syn/corpus.packed" "$WORK/syn/out" > /dev/null
	if diff -r -q "$WORK/syn/in" "$WORK/syn/out" > /dev/null 2>&1; then
		ok "packed round-trip"
	else
		bad "packed round-trip"
	fi

	# 2. the same stream, read back at each real variant's table offsets
	for v in ki1 ki1-p47 ki2; do
		mkdir -p "$WORK/syn/$v"
		fixtures/mkromimage "$WORK/syn/corpus.packed" "$v" "$WORK/syn/$v.u98"
		"$KIPACK" unpack -t "$v" "$WORK/syn/$v.u98" "$WORK/syn/$v" > /dev/null
		if diff -r -q "$WORK/syn/in" "$WORK/syn/$v" > /dev/null 2>&1; then
			ok "variant $v"
		else
			bad "variant $v"
		fi
	done

	# 3. the filename heuristic picks the same variant as -t, one arm per
	#    branch of kipack_variant_sniff(). The tests there are order-sensitive
	#    (ki1-p47.u98 does NOT match the "ki-p47" prefix and must fall through
	#    to ki1), so each arm gets its own check rather than one sample.
	sniff_case ki2-synth.u98    "$WORK/syn/ki2.u98"        # "ki2-"   -> ki2
	sniff_case ki-p47-synth.u98 "$WORK/syn/ki1-p47.u98"    # "ki-p47" -> ki1-p47
	sniff_case ki-l15d-synth.u98 "$WORK/syn/ki1.u98"       # no match -> ki1
	sniff_case synth.packed     "$WORK/syn/corpus.packed"  # ".packed" -> packed

	# 4. malformed input must be refused, not looped on.
	#
	# All three of these hung the decoder indefinitely before the guards went
	# in, so they are the regression tests for it. The offsets are fixed by the
	# format: the stream starts at KIPACK_PACKED_DATA (0x180) with a 16-bit
	# magic and an 8-bit file count, which is 24 bits exactly, so the first
	# file's 8-bit type lands on 0x183 and its 32-bit size on 0x184..0x187,
	# byte-aligned and little-endian.
	cp "$WORK/syn/corpus.packed" "$WORK/syn/bad-magic.packed"
	poke "$WORK/syn/bad-magic.packed" 384 '\377'
	reject_case "bad magic" "$WORK/syn/bad-magic.packed"

	cp "$WORK/syn/corpus.packed" "$WORK/syn/zero-files.packed"
	poke "$WORK/syn/zero-files.packed" 386 '\000'
	reject_case "zero file count" "$WORK/syn/zero-files.packed"

	# Only the size field's low byte is touched, so this holds whatever the
	# corpus happens to be: 0x01 is odd, hence never a multiple of 4.
	cp "$WORK/syn/corpus.packed" "$WORK/syn/odd-size.packed"
	poke "$WORK/syn/odd-size.packed" 388 '\001'
	reject_case "size not a multiple of 4" "$WORK/syn/odd-size.packed"

	# 5. pack refuses a segment that is not whole 32-bit instructions. This is
	#    the other half of the size guard - without it, pack writes a byte count
	#    into the header while encoding only size/4 instructions.
	mkdir -p "$WORK/syn/odd-in"
	printf '\000\000\000\000\000\000' > "$WORK/syn/odd-in/rom-0.bin"
	printf '\000\000\001\010'         > "$WORK/syn/odd-in/rom-0.addr"
	if run_bounded "$KIPACK" pack "$WORK/syn/odd-in" "$WORK/syn/odd.packed"; then
		bad "pack rejects a 6-byte segment"
	else
		ok "pack rejects a 6-byte segment"
	fi
}

check_synthetic
echo

# ---------------------------------------------------------------- ROM checks
rom_count=$(ls "$ROMS"/*.u98 2>/dev/null | wc -l | tr -d ' ')

if [ "$rom_count" = "0" ]; then
	# The manifests are derived from the ROMs. With none present there is
	# nothing to derive them from, so say so loudly rather than exiting 0 and
	# letting the caller believe the baselines were rewritten.
	if [ "$UPDATE" = "1" ]; then
		echo "check: --update needs the ROMs, but no $ROMS/*.u98 are present" >&2
		echo "check: the baselines were NOT updated" >&2
		exit 1
	fi
	echo "roms:      $ROMS/*.u98 not present - skipping ROM checks"
else
	# Unpack every ROM once; everything below reuses the result.
	for f in "$ROMS"/*.u98; do
		r=$(basename "$f" .u98)
		mkdir -p "$WORK/a/$r"
		"$KIPACK" unpack "$f" "$WORK/a/$r" > /dev/null
		"$KIPACK" pack "$WORK/a/$r" "$WORK/$r.packed" > /dev/null
	done

	for f in "$ROMS"/*.u98; do
		r=$(basename "$f" .u98)
		( cd "$WORK/a/$r" && sha rom-*.bin rom-*.addr ) | sed "s|  |  $r/|"
	done | sort > "$WORK/unpack.sha256"
	( cd "$WORK" && sha ./*.packed ) | sed 's|\./||' | sort > "$WORK/pack.sha256"

	if [ "$UPDATE" = "1" ]; then
		# The synthetic checks run before this point precisely so a broken
		# codec cannot overwrite the goldens with its own output and then
		# report success. Refuse to rebaseline from a red tree.
		if [ "$fail" != "0" ]; then
			echo "check: $fail check(s) failed - refusing to update the baselines" >&2
			exit 1
		fi
		mkdir -p "$EXPECTED"
		cp "$WORK/unpack.sha256" "$EXPECTED/unpack.sha256"
		cp "$WORK/pack.sha256" "$EXPECTED/pack.sha256"
		echo "baselines updated from $rom_count ROMs"
		exit 0
	fi

	echo "baselines:"
	if diff -u "$EXPECTED/unpack.sha256" "$WORK/unpack.sha256" > "$WORK/d1"; then
		ok "unpack ($rom_count ROMs)"
	else
		bad "unpack ($rom_count ROMs)"; cat "$WORK/d1"
	fi
	if diff -u "$EXPECTED/pack.sha256" "$WORK/pack.sha256" > "$WORK/d2"; then
		ok "pack ($rom_count ROMs)"
	else
		bad "pack ($rom_count ROMs)"; cat "$WORK/d2"
	fi

	echo "round-trip:"
	for f in "$ROMS"/*.u98; do
		r=$(basename "$f" .u98)
		mkdir -p "$WORK/c/$r"
		"$KIPACK" unpack -t packed "$WORK/$r.packed" "$WORK/c/$r" > /dev/null
		if diff -r -q "$WORK/a/$r" "$WORK/c/$r" > /dev/null 2>&1; then ok "$r"; else bad "$r"; fi
	done

	echo "size sweep:"
	if ROMS="$ROMS" fixtures/roundtrip-sizes.sh > "$WORK/sizes.log" 2>&1; then
		ok "4..4096 instructions"
	else
		bad "4..4096 instructions"
		cat "$WORK/sizes.log"
	fi
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = "0" ]
