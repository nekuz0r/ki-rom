/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _DRAW_H_
#define _DRAW_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t width;
    uint32_t height;
} image_t;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t reserved;
} animated_image_t;

/**
 * The framebuffer is RGB555 with red in the low bits: red | green << 5 | blue << 10.
 * Bit 15 is not wired to the DAC, so 0xFFFF is a value no visible pixel can hold.
 * That is what makes it usable both as "key nothing out" and as "no colour".
 */
#define RGB555(r, g, b) ((uint16_t)(((uint16_t)(b) << 10) | ((uint16_t)(g) << 5) | (uint16_t)(r)))

#define CHROMA_KEY_NONE (-1)
#define CHROMA_KEY_MAGENTA (0x7C1F) // What the character sprites key out on.
#define COLOR_NONE (0xFFFF)

/**
 * Optional per-blit transforms for draw_image_ex(). A zeroed struct is a plain
 * 1:1 copy, so `&(blit_t){ .zoom = 2 }` reads as "the default, but doubled".
 */
typedef struct
{
    uint8_t zoom;  // 0 or 1: 1:1. 2: double size, and so on.
    uint8_t shade; // 0 or 8: untouched. Otherwise every channel is scaled by shade/8.
    uint16_t rows; // 0: the whole image. Otherwise only the first N source rows.
    uint16_t flat; // The colour every non-keyed pixel takes when `flatten` is set.
    bool flatten;  // Draw the image as a silhouette in `flat`. Black is a useful
                   // silhouette, so this cannot be folded into `flat` -- a zeroed
                   // blit_t has to mean "no transform", not "paint it black".
    bool mirror;   // Flip horizontally.
    bool flip_y;   // Flip vertically. Combined with `rows` this yields a reflection.
} blit_t;

#define FADE_SPEED_1S (1)
#define FADE_SPEED_2S (2)
#define FADE_SPEED_3S (3)
#define FADE_SPEED_4S (4)
#define FADE_SPEED_5S (5)
#define FADE_SPEED_6S (6)
#define FADE_SPEED_7S (7)
#define FADE_SPEED_8S (8)
#define FADE_SPEED_9S (9)
#define FADE_SPEED_10S (10)

void draw_point(const uint16_t x, const uint16_t y);
void draw_horizontal_line(uint16_t x, uint16_t y, uint16_t length, uint16_t color);
void draw_vertical_line(uint16_t x, uint16_t y, uint16_t length);
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void draw_box(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1);
void draw_fill(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, uint16_t color);
void draw_gradient(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, uint16_t top, uint16_t bottom);
void draw_fill_darken(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1, const uint8_t shift);
void draw_image(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key);
void draw_image_mirror_x(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key);
void draw_image_ex(const int16_t x, const int16_t y, const image_t *img, uint16_t chroma_key, const blit_t *opt);
void draw_animation(const uint16_t x, const uint16_t y, const animated_image_t *img, uint16_t chroma_key);

uint16_t color_shade(uint16_t color, uint8_t shade);
uint16_t color_lerp(uint16_t from, uint16_t to, uint8_t t);
uint16_t color_fade_in_out(uint16_t from, uint16_t to, uint8_t speed);

#endif
