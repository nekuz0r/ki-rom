/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _MEM_H_
#define _MEM_H_

#include <stdint.h>
#include <stddef.h>
#include <umm_malloc/umm_malloc.h>

inline uint16_t __attribute__((always_inline)) swap_uint16(uint16_t val)
{
    return (val << 8) | (val >> 8);
}

inline int16_t __attribute__((always_inline)) swap_int16(int16_t val)
{
    return (val << 8) | ((val >> 8) & 0xFF);
}

inline uint32_t __attribute__((always_inline)) swap_uint32(uint32_t val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

inline int32_t __attribute__((always_inline)) swap_int32(int32_t val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | ((val >> 16) & 0xFFFF);
}

inline int64_t __attribute__((always_inline)) swap_int64(int64_t val)
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    return (val << 32) | ((val >> 32) & 0xFFFFFFFFULL);
}

inline uint64_t __attribute__((always_inline)) swap_uint64(uint64_t val)
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    return (val << 32) | (val >> 32);
}

inline void *__attribute__((always_inline)) align_down(register const void *const addr, register const uintptr_t align)
{
    return (void *)((uintptr_t)addr & -align);
}

inline void *__attribute__((always_inline)) align_up(register const void *const addr, register const uintptr_t align)
{
    return (void *)(((uintptr_t)addr + (align - 1)) & -align); // Round up to align-byte boundary
}

inline uint8_t __attribute__((always_inline)) is_aligned(register const void *const addr, register const uintptr_t align)
{
    return align_down(addr, align) == addr;
}

void *memcpy(uint8_t *dst, const uint8_t *src, size_t size);
void *memset(uint8_t *dst, uint64_t value, size_t count);

inline void *__attribute__((always_inline)) malloc(size_t size)
{
    return umm_malloc(size);
}

inline void __attribute__((always_inline)) free(void *ptr)
{
    return umm_free(ptr);
}

inline void *__attribute__((always_inline)) calloc(size_t num, size_t size)
{
    return umm_calloc(num, size);
}

inline void *__attribute__((always_inline)) realloc(void *ptr, size_t size)
{
    return umm_realloc(ptr, size);
}

#endif
