/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stddef.h>
#include <stdint.h>
#include "mem.h"

/*
 * umm_malloc declares these two as extern variables and reads them from
 * umm_multi_init(). Nothing in libs/umm_malloc defines them, so without a
 * definition the linker silently resolves both to 0 -- OUTPUT_FORMAT(binary)
 * suppresses the undefined-symbol error that would otherwise be raised.
 *
 * Defined here rather than by patching the vendored library. The values match
 * what start.S already passes to umm_init_heap(), so umm_init() becomes a
 * working equivalent rather than a trap.
 */
extern uint8_t _heap_vma[];
extern uint8_t _heap_size[];

void *UMM_MALLOC_CFG_HEAP_ADDR = _heap_vma;
uint32_t UMM_MALLOC_CFG_HEAP_SIZE = (uint32_t)(uintptr_t)_heap_size;

void *memcpy(uint8_t *dst, const uint8_t *src, size_t size)
{
    void *odst = dst;

    if (is_aligned(dst, sizeof(uint64_t)) && is_aligned(src, sizeof(uint64_t)))
    {
        while (size >= sizeof(uint64_t))
        {
            *(uint64_t *)dst = *(uint64_t *)src;
            dst += sizeof(uint64_t);
            src += sizeof(uint64_t);
            size -= sizeof(uint64_t);
        }
    }

    if (is_aligned(dst, sizeof(uint32_t)) && is_aligned(src, sizeof(uint32_t)))
    {
        while (size >= sizeof(uint32_t))
        {
            *(uint32_t *)dst = *(uint32_t *)src;
            dst += sizeof(uint32_t);
            src += sizeof(uint32_t);
            size -= sizeof(uint32_t);
        }
    }

    if (is_aligned(dst, sizeof(uint16_t)) && is_aligned(src, sizeof(uint16_t)))
    {
        while (size >= sizeof(uint16_t))
        {
            *(uint16_t *)dst = *(uint16_t *)src;
            dst += sizeof(uint16_t);
            src += sizeof(uint16_t);
            size -= sizeof(uint16_t);
        }
    }

    while (size > 0)
    {
        *(uint8_t *)dst++ = *(uint8_t *)src++;
        size--;
    }

    return odst;
}

void *memmove(uint8_t *dst, const uint8_t *src, size_t size)
{
    if (dst == src || size == 0)
    {
        return dst;
    }

    // Either disjoint, or dst below src: copying forwards is safe, so reuse
    // memcpy and its wider accesses.
    if (dst < src || dst >= src + size)
    {
        return memcpy(dst, src, size);
    }

    // Overlapping with dst above src: copy backwards so the source bytes are
    // read before they are overwritten.
    uint8_t *d = dst + size;
    const uint8_t *s = src + size;

    while (size > 0)
    {
        *--d = *--s;
        size--;
    }

    return dst;
}

void *memset(uint8_t *dst, uint64_t value, size_t count)
{
    void *odst = dst;
    value = (value & 0xFF) * 0x0101010101010101ULL;

    if (is_aligned(dst, sizeof(uint64_t)))
    {
        while (count >= sizeof(uint64_t))
        {
            *(uint64_t *)dst = value;
            dst += sizeof(uint64_t);
            count -= sizeof(uint64_t);
        }
    }

    if (is_aligned(dst, sizeof(uint32_t)))
    {
        while (count >= sizeof(uint32_t))
        {
            *(uint32_t *)dst = value;
            dst += sizeof(uint32_t);
            count -= sizeof(uint32_t);
        }
    }

    if (is_aligned(dst, sizeof(uint16_t)))
    {
        while (count >= sizeof(uint16_t))
        {
            *(uint16_t *)dst = value;
            dst += sizeof(uint16_t);
            count -= sizeof(uint16_t);
        }
    }

    // Set remaining bytes
    while (count > 0)
    {
        *(uint8_t *)dst++ = value;
        count -= sizeof(char);
    }

    return odst;
}
