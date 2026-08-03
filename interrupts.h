/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _INTERRUPTS_H_
#define _INTERRUPTS_H_

#include <stdint.h>

inline void __attribute__((always_inline)) interrupts_enable(void)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    status |= 1;
    asm volatile("mtc0 %0,$12" : : "r"(status) : "memory");
}

inline void __attribute__((always_inline)) interrupts_disable(void)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    status &= ~1;
    asm volatile("mtc0 %0,$12" : : "r"(status) : "memory");
}

/*
 * Nesting-safe pair. interrupts_disable()/interrupts_enable() clobber Status.IE
 * unconditionally, so an inner critical section re-enables interrupts on exit
 * even when the caller had them off. Use these instead wherever a section can
 * be entered with interrupts already disabled.
 */
inline uint32_t __attribute__((always_inline)) interrupts_save(void)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    asm volatile("mtc0 %0,$12" : : "r"(status & ~1u) : "memory");
    return status;
}

inline void __attribute__((always_inline)) interrupts_restore(register const uint32_t saved)
{
    register uint32_t status;
    asm volatile("mfc0 %0,$12" : "=r"(status) : : "memory");
    status = (status & ~1u) | (saved & 1u);
    asm volatile("mtc0 %0,$12" : : "r"(status) : "memory");
}

#endif
