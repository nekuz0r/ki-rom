/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdlib.h>
#include "draw.h"
#include "video.h"

extern uint16_t _fgcolor;
extern uint16_t _bgcolor;

void draw_image(register const uint16_t x, register const uint16_t y, register const image_t *img)
{
    register const uint32_t width = img->width;
    register const uint32_t height = img->height;
    register const void *ptr = (void *)img + 8;

    // register uint16_t *image = (uint16_t *)ptr;
    // for (register uint32_t cy = 0; cy < height; cy++)
    // {
    //     for (register uint32_t cx = 0; cx < width; cx++)
    //     {
    //         register uint16_t *pixel = video_get_ptr(x + cx, y + cy);
    //         *pixel = *image++;
    //     }
    // }

    for (register uint32_t cy = 0; cy < height; cy++)
    {
        register uint32_t cx = 0;
        register const void *const alignedPtr = align_up(ptr, 8);
        if (ptr != alignedPtr)
        {
            // How much pixel to reach alignment ?
            register uint32_t offset = alignedPtr - ptr;
            register uint32_t pixelOffset = offset >> 1;

            // Create a video block with those
            register uint64_t block = 0;
            for (register uint32_t px = 0; px < pixelOffset; px++)
            {
                block |= ((uint64_t)(*(uint16_t *)ptr)) << (px << 4);
                ptr += 2;
            }
            video_write_block(x + cx, y + cy, block, 0xFFFFFFFFFFFFFFFFull << (pixelOffset << 4));
            cx += pixelOffset;
        }

        // copy full block until less than 8 bytes
        while (width - cx >= 4)
        {
            register uint64_t block = *(uint64_t *)ptr;
            video_write_block(x + cx, y + cy, block, 0x0);
            ptr += 8;
            cx += 4;
        }

        // Create a video block with remaining pixels
        if (width - cx > 0)
        {
            register uint64_t block = 0x0;
            for (register uint32_t px = 0; px < width - cx; px++)
            {
                block |= ((uint64_t)(*(uint16_t *)ptr)) << (px << 4);
                ptr += 2;
            }
            video_write_block(x + cx, y + cy, block, 0xFFFFFFFFFFFFFFFFull << ((width - cx) << 4));
        }
    }
}

void draw_point(register const uint16_t x, register const uint16_t y)
{
    register uint16_t *pixel = video_get_ptr(x, y);
    *pixel = _fgcolor;
}

void draw_horizontal_line(register uint16_t x, register uint16_t y, register uint16_t length, register uint16_t color)
{
    register uint16_t *ptr = video_get_ptr(x, y);
    register uint16_t *alignedPtr = align_up(ptr, 8);

    while (ptr != alignedPtr && length > 0)
    {
        *ptr = color;
        length--;
        ptr++;
    }

    register uint64_t color64 = ((uint32_t)color << 16) | (uint32_t)color;
    color64 |= color64 << 32;
    while (length >= 4)
    {
        *((uint64_t *)ptr) = color64;
        length -= 4;
        ptr += 4;
    }

    while (length > 0)
    {
        *ptr = color;
        length--;
        ptr++;
    }
}

void draw_vertical_line(register uint16_t x, register uint16_t y, register uint16_t length)
{
    register uint16_t *ptr = video_get_ptr(x, y);
    while (length > 0)
    {
        *ptr = _fgcolor;
        length--;
        ptr += 320;
    }
}

void draw_line(register uint16_t x0, register uint16_t y0, register uint16_t x1, register uint16_t y1)
{
    register int16_t dx = abs(x1 - x0);
    register int16_t dy = -abs(y1 - y0);
    register int8_t sx = x0 < x1 ? 1 : -1;
    register int8_t sy = y0 < y1 ? 1 : -1;
    register int16_t error = dx + dy;

    while (1)
    {
        draw_point(x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        register int16_t e2 = 2 * error;
        if (e2 >= dy)
        {
            if (x0 == x1)
            {
                break;
            }
            error += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            if (y0 == y1)
            {
                break;
            }
            error += dx;
            y0 += sy;
        }
    }
}

void draw_box(register const uint16_t x0, register const uint16_t y0, register const uint16_t x1, register const uint16_t y1)
{
    draw_horizontal_line(x0, y0, x1 - x0, _fgcolor);
    draw_horizontal_line(x0, y1, x1 - x0, _fgcolor);
    draw_vertical_line(x0, y0, y1 - y0);
    draw_vertical_line(x1, y0, y1 - y0);

    if (_bgcolor != 0xAAAA)
    {
        for (register uint16_t line = 1; line < (y1 - y0); line++)
        {
            draw_horizontal_line(x0 + 1, y0 + line, x1 - x0 - 1, _bgcolor);
        }
    }
}
