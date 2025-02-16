/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _DELAY_H_
#define _DELAY_H_

/*
li      v0,us       (1 cycle)
nop                 (1 cycle x us)
addiu	v0,v0,-1    (1 cycle x us)
bnez	v0,0xc      (2 cycles x us)
nop

Input clock is 1/2 of pipeline frequency (R4600/R4700 datasheet page 1 - 3 and page 1 - 5)
1us = (x * 4cycles) / (50Mhz * 2) => x = 100 / 4 = 25
*/
inline void __attribute__((always_inline)) udelay(uint32_t us)
{
    for (uint32_t i = (us * 25); i > 0; i--)
    {
        asm volatile("nop");
    }
}

inline void __attribute__((always_inline)) delay(uint32_t ms)
{
    udelay(ms * 1000);
}

#endif
