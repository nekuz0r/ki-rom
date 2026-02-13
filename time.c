/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "time.h"
#include "math.h"

static uint64_t _ticks = 0;

void time_init(void)
{
    register uint32_t t;
    asm volatile("mfc0 %0,$9" : "=r"(t));
    _ticks = t;
}

uint64_t ticks(void)
{
    /**
     * The coprocessor 0 register 0x9 (Count) is a 32 bit register
     */
    register uint32_t t;
    // if (ticks == -1)
    // {
    //     asm volatile("mfc0 %0,$9" : "=r"(ticks));
    // }
    asm volatile("mfc0 %0,$9" : "=r"(t));
    _ticks += (uint32_t)(t - (uint32_t)_ticks);
    return _ticks;
}

uint64_t clock(void)
{
    return ticks() / 50ull;
}

uint64_t millis(void)
{
    return clock() / 1000ul;
}

uint64_t micros(void)
{
    return clock();
}
