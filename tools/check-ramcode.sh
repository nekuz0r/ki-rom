#!/bin/sh
# SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
# SPDX-License-Identifier: AGPL-3.0-only
#
# Verifies that every RAMCODE_FN function is linked into SRAM and never reaches
# back into the ROM window.
#
# A .ramcode function keeps running while the ROM window holds a different
# image, so one reach into ROM -- a libgcc helper pulled in by a division, a
# forgotten wdt_reset() -- is fatal. Nothing else catches it: the image links
# and runs correctly right up until the bank flips.
#
# Usage: tools/check-ramcode.sh build/19489-ki-l15d.elf

set -eu

ELF="${1:-}"
if [ -z "$ELF" ] || [ ! -f "$ELF" ]; then
    echo "usage: $0 <elf>" >&2
    exit 2
fi

OBJDUMP="${OBJDUMP:-mipsel-linux-gnu-objdump}"

# Every function that must be SRAM-resident. Each name must resolve; a stale or
# empty list would let this check pass without verifying anything.
RAMCODE_FNS="bootrom_swap"

SRAM_LO=$(printf '%d' 0x80000000)
SRAM_HI=$(printf '%d' 0x80080000)

syms=$("$OBJDUMP" -t "$ELF")
fail=0

for fn in $RAMCODE_FNS; do
    fn_fail=0

    # objdump -t prints "<addr> <flags> F <section>\t<size> <name>". Take the
    # name as the last field and the size as the one before it, so the variable
    # width of the flag column does not matter.
    set -- $(echo "$syms" | awk -v n="$fn" '$NF == n { print $1, $(NF - 1) }')
    if [ $# -ne 2 ]; then
        echo "FAIL $ELF: $fn: no such symbol (expected a RAMCODE_FN function)" >&2
        fail=1
        continue
    fi
    addr="$1"
    size=$(printf '%d' "0x$2")
    addr_dec=$(printf '%d' "0x$addr")

    if [ "$addr_dec" -lt "$SRAM_LO" ] || [ "$addr_dec" -ge "$SRAM_HI" ]; then
        echo "FAIL $ELF: $fn: linked at 0x$addr, outside SRAM" >&2
        fn_fail=1
    fi

    if [ "$size" -eq 0 ]; then
        echo "FAIL $ELF: $fn: zero-sized symbol" >&2
        fn_fail=1
    fi

    clones=$(echo "$syms" | awk -v p="$fn." 'index($NF, p) == 1 { print $NF }')
    if [ -n "$clones" ]; then
        echo "FAIL $ELF: $fn: GCC emitted a clone: $clones (missing noclone?)" >&2
        fn_fail=1
    fi

    if [ "$fn_fail" -eq 0 ]; then
        dis=$("$OBJDUMP" -D --start-address="0x$addr" \
                            --stop-address="$((addr_dec + size))" "$ELF")

        # Any call leaves the routine, and every other function lives in ROM.
        calls=$(echo "$dis" | grep -E '\b(jal|jalr|bal)\b' || true)
        if [ -n "$calls" ]; then
            echo "FAIL $ELF: $fn: calls out of .ramcode:" >&2
            echo "$calls" >&2
            fn_fail=1
        fi

        # Catches what the rule above cannot: a tail call compiled to
        # "lui t9,0x9fc3; jr t9", and any load of a ROM address. A lui only
        # ever prints the upper halfword (four hex digits), never the full
        # 8-digit address, so this matches the whole 0x9fc prefix rather than
        # trying to bound it exactly at 512K -- nothing valid in this image
        # lives at 0x9fcXXXXX other than ROM.
        rom=$(echo "$dis" | grep -Ei '(\b9fc[0-9a-f]{5}\b|0x9fc[0-9a-f]\b)' || true)
        if [ -n "$rom" ]; then
            echo "FAIL $ELF: $fn: references the ROM window:" >&2
            echo "$rom" >&2
            fn_fail=1
        fi
    fi

    if [ "$fn_fail" -eq 0 ]; then
        echo "ok   $ELF: $fn at 0x$addr ($size bytes), SRAM-resident"
    else
        fail=1
    fi
done

# Reverse check: every symbol GCC actually placed in a .ramcode input section
# must be named in RAMCODE_FNS above, or a RAMCODE_FN function silently goes
# unchecked -- the exact staleness the comment on RAMCODE_FNS warns about.
# The linked ELF cannot answer this: boot.ld merges .ramcode into .data, so
# the input-section identity is gone by the time the loop above runs. The
# object files still carry it, so look there instead.
objs="${ELF%.elf}-*.o"
for obj in $objs; do
    [ -f "$obj" ] || continue

    tagged=$("$OBJDUMP" -t "$obj" | awk 'NF >= 3 && $(NF - 2) == ".ramcode" && $(NF - 1) != "00000000" { print $NF }')
    for sym in $tagged; do
        known=0
        for fn in $RAMCODE_FNS; do
            [ "$sym" = "$fn" ] && known=1 && break
        done
        if [ "$known" -eq 0 ]; then
            echo "FAIL $ELF: $sym: tagged RAMCODE_FN in $obj but missing from RAMCODE_FNS" >&2
            fail=1
        fi
    done
done

exit "$fail"
