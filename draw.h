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

void draw_point(register const uint16_t x, register const uint16_t y);
void draw_horizontal_line(register uint16_t x, register uint16_t y, register uint16_t length, register uint16_t color);
void draw_vertical_line(register uint16_t x, register uint16_t y, register uint16_t length);
void draw_box(register const uint16_t x0, register const uint16_t y0, register const uint16_t x1, register const uint16_t y1);
void draw_image(register const uint16_t x, register const uint16_t y, register const image_t *img);

#endif
