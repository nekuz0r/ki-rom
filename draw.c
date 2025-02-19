/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdlib.h>
#include "draw.h"
#include "video.h"

extern uint16_t _fgcolor;
extern uint16_t _bgcolor;

void draw_image(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key)
{
    const uint32_t width = img->width;
    const uint32_t height = img->height;
    const void *ptr = (void *)img + 8;
    uint64_t block = 0x0;
    uint64_t mask = 0;

    //  uint16_t *image = (uint16_t *)ptr;
    // for ( uint32_t cy = 0; cy < height; cy++)
    // {
    //     for ( uint32_t cx = 0; cx < width; cx++)
    //     {
    //          uint16_t *pixel = video_get_ptr(x + cx, y + cy);
    //         *pixel = *image++;
    //     }
    // }

    for (uint32_t cy = 0; cy < height; cy++)
    {
        uint32_t cx = 0;
        const void *const alignedPtr = align_up(ptr, 8);
        if (ptr != alignedPtr)
        {
            // How much pixel to reach alignment ?
            uint32_t offset = alignedPtr - ptr;
            uint32_t pixelOffset = offset >> 1;
            block = 0;
            mask = 0;

            // Create a video block with those
            for (uint32_t px = 0; px < pixelOffset; px++)
            {
                uint16_t pixelColor = *(uint16_t *)ptr;
                block |= ((uint64_t)(pixelColor)) << (px << 4);
                mask |= ((uint16_t)(pixelColor != chroma_key) - 1) << (px << 4);

                ptr += 2;
            }
            video_write_block(x + cx, y + cy, block, mask);
            cx += pixelOffset;
        }

        // copy full block until less than 8 bytes
        mask = 0;
        while (width - cx >= 4)
        {
            block = *(uint64_t *)ptr;
            if (chroma_key != 0xFFFF)
            {
                mask = (uint16_t)((((uint16_t *)ptr)[3] != chroma_key) - 1);
                mask <<= 16;
                mask |= (uint16_t)((((uint16_t *)ptr)[2] != chroma_key) - 1);
                mask <<= 16;
                mask |= (uint16_t)((((uint16_t *)ptr)[1] != chroma_key) - 1);
                mask <<= 16;
                mask |= (uint16_t)((((uint16_t *)ptr)[0] != chroma_key) - 1);
            }

            video_write_block(x + cx, y + cy, block, mask);
            ptr += 8;
            cx += 4;
        }

        // Create a video block with remaining pixels
        if (width - cx > 0)
        {
            block = 0x0;
            mask = 0;
            for (uint32_t px = 0; px < width - cx; px++)
            {
                uint16_t pixelColor = *(uint16_t *)ptr;
                block |= ((uint64_t)(pixelColor)) << (px << 4);
                mask |= ((uint16_t)(pixelColor != chroma_key) - 1) << (px << 4);
                ptr += 2;
            }
            video_write_block(x + cx, y + cy, block, mask);
        }
    }
}

void draw_point(const uint16_t x, const uint16_t y)
{
    uint16_t *pixel = video_get_ptr(x, y);
    *pixel = _fgcolor;
}

void draw_horizontal_line(uint16_t x, uint16_t y, uint16_t length, uint16_t color)
{
    uint16_t *ptr = video_get_ptr(x, y);
    uint16_t *alignedPtr = align_up(ptr, 8);

    while (ptr != alignedPtr && length > 0)
    {
        *ptr = color;
        length--;
        ptr++;
    }

    uint64_t color64 = ((uint32_t)color << 16) | (uint32_t)color;
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

void draw_vertical_line(uint16_t x, uint16_t y, uint16_t length)
{
    uint16_t *ptr = video_get_ptr(x, y);
    while (length > 0)
    {
        *ptr = _fgcolor;
        length--;
        ptr += 320;
    }
}

void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int8_t sx = x0 < x1 ? 1 : -1;
    int8_t sy = y0 < y1 ? 1 : -1;
    int16_t error = dx + dy;

    while (1)
    {
        draw_point(x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        int16_t e2 = 2 * error;
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

void draw_box(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1)
{
    draw_horizontal_line(x0 + 1, y0, x1 - x0 - 1, _fgcolor);
    draw_horizontal_line(x0 + 1, y1, x1 - x0 - 1, _fgcolor);
    draw_vertical_line(x0, y0 + 1, y1 - y0 - 1);
    draw_vertical_line(x1, y0 + 1, y1 - y0 - 1);

    if (_bgcolor != 0xAAAA)
    {
        for (uint16_t line = 1; line < (y1 - y0); line++)
        {
            draw_horizontal_line(x0 + 1, y0 + line, x1 - x0 - 1, _bgcolor);
        }
    }
}
