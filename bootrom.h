/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _BOOTROM_H_
#define _BOOTROM_H_

#include <stdint.h>

/**
 * SECTOR_COUNT values for the bank-select command. The device maps the named
 * bank into the ROM window at 0x9FC00000.
 */
#define BOOTROM_BANK_KI1 (1)
#define BOOTROM_BANK_KI2 (2)

/**
 * Which bank this image belongs in, from the build variant -- the same KI/KI2
 * define view_main.c switches on. It is not read back from hardware, so a KI1
 * image flashed into bank 2 makes selecting KI1 a silent no-op that boots KI2.
 */
#if defined(KI)
#define BOOTROM_BANK_SELF BOOTROM_BANK_KI1
#else
#define BOOTROM_BANK_SELF BOOTROM_BANK_KI2
#endif

/**
 * Maps `bank` into the ROM window and enters the newly mapped image through its
 * own reset vector. Returns only when the bank is unchanged under the hardware
 * model bootrom.c documents -- so the caller can fall back to booting this
 * image. Any outcome where the bank might already have moved is resolved by
 * resetting into whichever image ended up mapped, never by returning.
 *
 * far: the caller is in ROM at 0x9FCxxxxx and this lives in SRAM at 0x8000xxxx.
 * jal encodes only the low 28 bits of its target and keeps the PC's top four,
 * so a near call cannot reach across. Same reason view.h's _reset carries it.
 */
void bootrom_swap(uint8_t bank) __attribute__((far));

#endif
