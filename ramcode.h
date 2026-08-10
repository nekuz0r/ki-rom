/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _RAMCODE_H_
#define _RAMCODE_H_

/**
 * Places a function in SRAM instead of the ROM window. boot.ld collects
 * .ramcode into .data, whose VMA is in SRAM and LMA in ROM, and start.S copies
 * the segment before main() runs -- the same route detour hooks take.
 *
 * For code that has to keep executing while the ROM window is unreadable or
 * holds a different image. Such a function may not reach into ROM at all:
 * everything it uses must be a macro or always_inline, and it must avoid
 * division and 64-bit arithmetic, which GCC turns into calls to libgcc.
 * tools/check-ramcode.sh enforces this on the linked image.
 *
 * noinline keeps it out of ROM-resident callers. noclone stops GCC emitting a
 * specialised copy into .text, which would be a ROM-resident duplicate of code
 * whose whole purpose is not being in ROM.
 */
#define RAMCODE_FN __attribute__((noinline, noclone, section(".ramcode")))

#endif
