/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _PRINT_H_
#define _PRINT_H_

#include <stdint.h>

void set_xy(register const uint16_t x, register const uint16_t y);
void set_text_color(register uint16_t fgcolor, register uint16_t bgcolor);
void print_xy(register const uint16_t x, register const uint16_t y, const char *str);
void print_str(register const char *str);
void print_dec(register int64_t val);
void print_hex(register uint64_t val, register uint8_t bits);

#endif
