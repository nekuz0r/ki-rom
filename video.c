/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include "video.h"
#include "io.h"
#include "cache.h"

extern uint16_t gVramBank0[FRAME_BUFFER_SIZE];
extern uint16_t gVramBank1[FRAME_BUFFER_SIZE];
uint16_t *gBackBuffer = gVramBank1;
uint64_t frame_counter = 0;

void video_init(void)
{
    gIO.vramControl = 0;
}

/*
 * Fill a 32-byte-aligned, whole-cache-line region.
 *
 * KSEG0 is write-back write-allocate, so a plain store to a line that is not
 * resident first reads that line from SRAM -- and here every byte of it is
 * immediately overwritten, so the fetch is pure waste. CREATE_DIRTY_EXCLUSIVE
 * claims the line as dirty without reading memory, which halves the bus
 * traffic. It is only safe because the four stores below write the whole line;
 * leaving any of it untouched would expose stale cache contents.
 */
static inline void video_fill_lines(uint64_t *dst, const uint64_t *end, uint64_t value)
{
    while (dst < end)
    {
        CACHE_OP(CREATE_DIRTY_EXCLUSIVE_D, dst, 0);
        dst[0] = value;
        dst[1] = value;
        dst[2] = value;
        dst[3] = value;
        dst += 4;
    }
}

void video_clear_vrams(void)
{
    // 0x50000 bytes across both banks: 10240 whole cache lines.
    video_fill_lines((uint64_t *)gVramBank0,
                     (uint64_t *)((uintptr_t)gVramBank1 + 0x28000),
                     0);
}

void video_clear_framebuffer(uint64_t color)
{
    // 0x25800 bytes of visible area: 4800 whole cache lines.
    video_fill_lines((uint64_t *)gBackBuffer,
                     (uint64_t *)((uintptr_t)gBackBuffer + 0x25800),
                     color);
}

void video_swap_buffers(void)
{
    if (gBackBuffer == gVramBank0)
    {
        gBackBuffer = gVramBank1;
        gIO.vramControl = 0;
    }
    else
    {
        gBackBuffer = gVramBank0;
        gIO.vramControl = 4;
    }
}

void video_vsync_wait(void)
{
    register uint32_t cause;
    do
    {
        asm volatile("mfc0 %0,$13"
                     : "=r"(cause));
    } while ((cause & 0x400) != 0);

    do
    {
        asm volatile("mfc0 %0,$13"
                     : "=r"(cause));
    } while ((cause & 0x400) == 0);

    frame_counter++;
}
