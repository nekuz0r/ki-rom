/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _INTERRUPTS_H_
#define _INTERRUPTS_H_

#include <stdint.h>

/*
 * interrupts_enable() and interrupts_disable() return Status as it was before
 * the change, so a caller can hand that value to interrupts_restore() and leave
 * IE exactly as it found it. This matters for any section that can be entered
 * with interrupts already off: unconditionally re-enabling on the way out would
 * let an interrupt in while an outer critical section is still running.
 *
 * Callers that do not need the previous state can ignore the return value.
 *
 * interrupts_restore() writes back only IE. Every other Status bit keeps its
 * current value, so restoring cannot undo an unrelated change made inside the
 * section.
 */
inline uint32_t __attribute__((always_inline)) interrupts_enable(void)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    asm volatile("mtc0 %0,$12" : : "r"(status | 1u) : "memory");
    return status;
}

inline uint32_t __attribute__((always_inline)) interrupts_disable(void)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    asm volatile("mtc0 %0,$12" : : "r"(status & ~1u) : "memory");
    return status;
}

inline void __attribute__((always_inline)) interrupts_restore(const uint32_t saved)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    status = (status & ~1u) | (saved & 1u);
    asm volatile("mtc0 %0,$12" : : "r"(status) : "memory");
}

#endif
