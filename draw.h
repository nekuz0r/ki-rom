/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _DRAW_H_
#define _DRAW_H_

#include <stdint.h>

typedef struct
{
    uint32_t width;
    uint32_t height;
} image_t;

#define CHROMA_KEY_NONE (-1)

void draw_point(const uint16_t x, const uint16_t y);
void draw_horizontal_line(uint16_t x, uint16_t y, uint16_t length, uint16_t color);
void draw_vertical_line(uint16_t x, uint16_t y, uint16_t length);
void draw_box(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1);
void draw_image(const uint16_t x, const uint16_t y, const image_t *img, uint16_t chroma_key);

#endif
