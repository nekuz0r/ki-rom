/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include "video.h"
#include "io.h"

extern uint16_t gVramBank0[FRAME_BUFFER_SIZE];
extern uint16_t gVramBank1[FRAME_BUFFER_SIZE];
uint16_t *gBackBuffer = gVramBank1;
uint64_t frame_counter = 0;

void video_init(void)
{
    gIO.vramControl = 0;
}

void video_clear_vrams(void)
{
    register uint64_t *ptr = (uint64_t *)gVramBank0;
    register uint64_t *end = (uint64_t *)((uintptr_t)gVramBank1 + 0x28000);

    while (ptr < end)
    {
        *ptr = 0;
        ptr++;
    }
}

void video_clear_framebuffer(uint64_t color)
{
    register uint64_t *dst = (uint64_t *)gBackBuffer;
    register uint64_t *end = (uint64_t *)((uintptr_t)gBackBuffer + 0x25800);
    while (dst < end)
    {
        *dst++ = color;
    }
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
