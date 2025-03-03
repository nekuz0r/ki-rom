/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <stdint.h>
#include "mem.h"

#define FRAME_BUFFER_SIZE 0x14000
#define FRAME_BUFFER_VISIBLE_SIZE 0x12C00

extern uint16_t *gBackBuffer;
extern uint64_t frame_counter;

void video_init(void);
void video_clear_vrams(void);
void video_clear_framebuffer(uint64_t color);
void video_swap_buffers(void);
void video_vsync_wait(void);

inline uint16_t *__attribute__((always_inline)) video_get_ptr(register const uint16_t x, register const uint16_t y)
{
    register uint32_t offset = x + (y * 320);
#if defined(VIDEO_STRICT_BOUND_CHECK)
    if (offset >= FRAME_BUFFER_VISIBLE_SIZE)
    {
        offset %= FRAME_BUFFER_VISIBLE_SIZE;
    }
#endif
    return gBackBuffer + offset;
}

inline void __attribute__((always_inline)) video_write_block(register const uint16_t x, register const uint16_t y, register uint64_t block, register uint64_t mask)
{
    register uint64_t *ptr = (uint64_t *)video_get_ptr(x, y);
    register uint64_t *alignedPtr = align_down(ptr, 8);

    // are we 64 bits aligned ?
    if (ptr == alignedPtr)
    {
        *ptr &= mask;
        *ptr |= (block & ~mask);
        return;
    }

    // Not aligned
    register const uint8_t offset = ((uintptr_t)ptr - (uintptr_t)alignedPtr) << 3;
    register const uint8_t rightOffset = 64 - offset;
    register const uint64_t block_carry = block >> rightOffset;
    register uint64_t block_mask_carry = mask >> rightOffset;

    block <<= offset;
    mask <<= offset;
    mask |= 0xFFFFFFFFFFFFFFFFull >> rightOffset;
    block_mask_carry |= 0xFFFFFFFFFFFFFFFFull << offset;

    *alignedPtr &= mask;
    *alignedPtr |= (block & ~mask);
    alignedPtr += 1;
    *alignedPtr &= block_mask_carry;
    *alignedPtr |= (block_carry & ~block_mask_carry);
}

#endif
