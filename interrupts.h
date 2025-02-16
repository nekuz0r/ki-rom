/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _INTERRUPTS_H_
#define _INTERRUPTS_H_

inline void __attribute__((always_inline)) interrupts_enable(void)
{
    register uint64_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status));
    status |= 1;
    asm volatile("mtc0 %0,$12" : : "r"(status));
}

inline void __attribute__((always_inline)) interrupts_disable(void)
{
    register uint64_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status));
    status |= 1;
    status ^= 1;
    asm volatile("mtc0 %0,$12" : : "r"(status));
}

#endif
