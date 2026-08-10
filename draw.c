/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "math.h"
#include "draw.h"
#include "video.h"

extern uint16_t _fgcolor;
extern uint16_t _bgcolor;
extern uint64_t frame_counter;

#ifdef DRAW_IMAGE_OPTIMIZED
static void _draw_image(const uint16_t x, const uint16_t y, const uint32_t width, const uint32_t height, const uint8_t *ptr, uint16_t chroma_key)
{
    uint64_t block = 0x0;
    uint64_t mask = 0;

    for (uint32_t cy = 0; cy < height; cy++)
    {
        uint32_t cx = 0;
        const uint8_t *const alignedPtr = align_up(ptr, 8);
        if (ptr != alignedPtr)
        {
            // How much pixel to reach alignment ?
            uint32_t offset = alignedPtr - ptr;
            uint32_t pixelOffset = offset >> 1;
            block = 0;
            mask = 0xFFFFFFFFFFFFFFFFull << (pixelOffset << 4);

            // Create a video block with those
            for (uint32_t px = 0; px < pixelOffset; px++)
            {
                uint16_t pixelColor = *(uint16_t *)ptr;
                block |= ((uint64_t)(pixelColor)) << (px << 4);
                mask |= ((uint64_t)((pixelColor != chroma_key) - 1) & 0xFFFF) << (px << 4);
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
            mask = 0xFFFFFFFFFFFFFFFFull << ((width - cx) << 4);
            for (uint32_t px = 0; px < width - cx; px++)
            {
                uint16_t pixelColor = *(uint16_t *)ptr;
                block |= ((uint64_t)(pixelColor)) << (px << 4);
                mask |= ((uint64_t)((pixelColor != chroma_key) - 1) & 0xFFFF) << (px << 4);
                ptr += 2;
            }
            video_write_block(x + cx, y + cy, block, mask);
        }
    }
}
#endif

void draw_animation(const uint16_t x, const uint16_t y, const animated_image_t *img, uint16_t chroma_key)
{
    const uint32_t width = img->width;
    const uint32_t height = img->height;
    const uint32_t frames = img->frames;
    const uint8_t *ptr = (uint8_t *)img + sizeof(animated_image_t);

    const uint32_t frame = (frame_counter % (6 * frames)) / 6;

    ptr += (frame * height * width * 2);

#ifdef DRAW_IMAGE_OPTIMIZED
    _draw_image(x, y, width, height, ptr, chroma_key);
#else
    uint16_t *image = (uint16_t *)ptr;
    for (uint32_t cy = 0; cy < height; cy++)
    {
        uint16_t *pixel = video_get_ptr(x, y + cy);
        for (uint32_t cx = 0; cx < width; cx++)
        {
            if (*image != chroma_key)
            {
                *pixel = *image;
            }
            image++;
            pixel++;
        }
    }
#endif
}

void draw_image_mirror_x(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key)
{
    const uint32_t width = img->width;
    const uint32_t height = img->height;
    const uint8_t *ptr = (uint8_t *)img + sizeof(image_t);
    uint16_t *image = (uint16_t *)ptr;

    for (uint32_t cy = 0; cy < height; cy++)
    {
        uint16_t *pixel = video_get_ptr(x, y + cy);
        image += width;
        uint16_t *src = image - 1;
        for (uint32_t cx = 0; cx < width; cx++)
        {
            if (*src != chroma_key)
            {
                *pixel = *src;
            }
            src--;
            pixel++;
        }
    }
}

void draw_image(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key)
{
    const uint32_t width = img->width;
    const uint32_t height = img->height;
    const uint8_t *ptr = (uint8_t *)img + sizeof(image_t);

#ifdef DRAW_IMAGE_OPTIMIZED
    _draw_image(x, y, width, height, ptr, chroma_key);
#else
    uint16_t *image = (uint16_t *)ptr;
    for (uint32_t cy = 0; cy < height; cy++)
    {
        uint16_t *pixel = video_get_ptr(x, y + cy);
        for (uint32_t cx = 0; cx < width; cx++)
        {
            if (*image != chroma_key)
            {
                *pixel = *image;
            }
            image++;
            pixel++;
        }
    }
#endif
}

void draw_image_ex(const int16_t x, const int16_t y, const image_t *img, uint16_t chroma_key, const blit_t *opt)
{
    const int32_t zoom = (opt->zoom < 2) ? 1 : (int32_t)opt->zoom;
    const int32_t srcw = (int32_t)img->width;
    const int32_t srch = (opt->rows != 0 && opt->rows < img->height) ? (int32_t)opt->rows : (int32_t)img->height;
    const uint16_t *const pixels = (const uint16_t *)((const uint8_t *)img + sizeof(image_t));

    // Clip in destination space first. video_get_ptr() has no bound check, so an
    // unclipped blit at a negative x does not draw off screen -- it converts to a
    // huge unsigned offset and scribbles over SRAM.
    int32_t dx0 = x, dy0 = y;
    int32_t dx1 = x + srcw * zoom;
    int32_t dy1 = y + srch * zoom;
    if (dx0 < 0)
    {
        dx0 = 0;
    }
    if (dy0 < 0)
    {
        dy0 = 0;
    }
    if (dx1 > 320)
    {
        dx1 = 320;
    }
    if (dy1 > 240)
    {
        dy1 = 240;
    }
    if (dx0 >= dx1 || dy0 >= dy1)
    {
        return;
    }

    // Map the clipped destination rectangle back to the source pixels that touch
    // it. Only the first and last of those can be partially visible, but clamping
    // every block costs two compares and keeps the two cases in one loop.
    const int32_t sx0 = (dx0 - x) / zoom;
    const int32_t sx1 = ((dx1 - 1 - x) / zoom) + 1;
    const int32_t sy0 = (dy0 - y) / zoom;
    const int32_t sy1 = ((dy1 - 1 - y) / zoom) + 1;

    const bool flatten = opt->flatten;
    const bool shade = (opt->shade != 0 && opt->shade != 8);

    for (int32_t sy = sy0; sy < sy1; sy++)
    {
        const int32_t row = opt->flip_y ? ((int32_t)img->height - 1 - sy) : sy;
        const uint16_t *const line = pixels + (row * srcw);

        const int32_t by = y + (sy * zoom);
        const int32_t ry0 = (by < dy0) ? dy0 : by;
        const int32_t ry1 = (by + zoom > dy1) ? dy1 : (by + zoom);

        for (int32_t sx = sx0; sx < sx1; sx++)
        {
            uint16_t color = line[opt->mirror ? (srcw - 1 - sx) : sx];
            if (color == chroma_key)
            {
                continue;
            }
            if (flatten)
            {
                color = opt->flat;
            }
            else if (shade)
            {
                color = color_shade(color, opt->shade);
            }

            const int32_t bx = x + (sx * zoom);
            const int32_t rx0 = (bx < dx0) ? dx0 : bx;
            const int32_t rx1 = (bx + zoom > dx1) ? dx1 : (bx + zoom);

            for (int32_t py = ry0; py < ry1; py++)
            {
                uint16_t *pixel = video_get_ptr(rx0, py);
                for (int32_t px = rx0; px < rx1; px++)
                {
                    *pixel++ = color;
                }
            }
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
    while (length >= 4)
    {
        ptr[0] = _fgcolor;
        ptr[320] = _fgcolor;
        ptr[640] = _fgcolor;
        ptr[960] = _fgcolor;
        ptr += 1280;
        length -= 4;
    }

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
    // A box needs a strictly positive extent. Without this guard the length
    // expressions below are evaluated as int and then converted to the
    // uint16_t length parameter, so x1 == x0 yields -1 -> 65535.
    if (x1 <= x0 || y1 <= y0)
    {
        return;
    }

    // The horizontals span the full width so the four corners are drawn.
    draw_horizontal_line(x0, y0, x1 - x0 + 1, _fgcolor);
    draw_horizontal_line(x0, y1, x1 - x0 + 1, _fgcolor);
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

void draw_fill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, uint16_t color)
{
    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    for (uint16_t y = y0; y <= y1; y++)
    {
        draw_horizontal_line(x0, y, x1 - x0 + 1, color);
    }
}

void draw_gradient(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, uint16_t top, uint16_t bottom)
{
    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    // One ramp step per scanline, so the whole gradient is just draw_fill() with
    // a colour that walks. A single-row gradient is its top colour.
    const uint16_t span = (y1 > y0) ? (y1 - y0) : 1;
    for (uint16_t y = y0; y <= y1; y++)
    {
        const uint8_t t = (uint8_t)(((uint32_t)(y - y0) * 255) / span);
        draw_horizontal_line(x0, y, x1 - x0 + 1, color_lerp(top, bottom, t));
    }
}

uint16_t color_shade(uint16_t color, uint8_t shade)
{
    uint16_t r = ((color & 0x1F) * shade) >> 3;
    uint16_t g = (((color >> 5) & 0x1F) * shade) >> 3;
    uint16_t b = (((color >> 10) & 0x1F) * shade) >> 3;

    // shade > 8 brightens, and a channel that saturates must clamp rather than
    // wrap into the next one.
    return RGB555(MIN(r, 31), MIN(g, 31), MIN(b, 31));
}

uint16_t color_lerp(uint16_t from, uint16_t to, uint8_t t)
{
    uint8_t ra = from & 0x1F;
    uint8_t rb = to & 0x1F;
    uint8_t ga = (from >> 5) & 0x1F;
    uint8_t gb = (to >> 5) & 0x1F;
    uint8_t ba = (from >> 10) & 0x1F;
    uint8_t bb = (to >> 10) & 0x1F;

    uint8_t r = ra + (((uint32_t)t * (int16_t)(rb - ra)) >> 8);
    uint8_t g = ga + (((uint32_t)t * (int16_t)(gb - ga)) >> 8);
    uint8_t b = ba + (((uint32_t)t * (int16_t)(bb - ba)) >> 8);

    return RGB555(r, g, b);
}

uint16_t color_fade_in_out(uint16_t from, uint16_t to, uint8_t speed)
{
    uint32_t res = (60 * speed);
    uint32_t pos = frame_counter % res;
    uint32_t half = res >> 1;
    uint32_t dist = (pos < half) ? (half - pos) : (pos - half);
    uint32_t t_fixed = (dist * 512) / res;

    if (t_fixed > 255)
    {
        t_fixed = 255;
    }

    return color_lerp(from, to, (uint8_t)t_fixed);
}
