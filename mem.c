/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stddef.h>
#include <stdint.h>
#include "mem.h"

void *memcpy(void *dst, const void *src, size_t size)
{
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
        *(char *)dst++ = *(char *)src++;
        size--;
    }
    return dst;
}

void *memset(void *dst, uint64_t value, size_t count)
{
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
        *(char *)dst++ = value;
        count -= sizeof(char);
    }
    return dst;
}
