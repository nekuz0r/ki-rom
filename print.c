/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stdarg.h>
#include "print.h"
#include "video.h"
#include "io.h"
#include "mem.h"
#include "font.h"

uint16_t _fgcolor = 0x7FFF;
uint16_t _bgcolor = 0xAAAA;
static uint16_t _x = 0;
static uint16_t _y = 0;

void set_text_color(register uint16_t fgcolor, register uint16_t bgcolor)
{
    _fgcolor = fgcolor;
    _bgcolor = bgcolor;
}

void set_xy(register const uint16_t x, register const uint16_t y)
{
    _x = x;
    _y = y;

#if defined(VIDEO_STRICT_BOUND_CHECK)
    if (_x >= 320)
    {
        _x %= 320;
    }
#endif
}

void putchar_(register const char c)
{
    if (c < 32 || c > 126)
    {
        return;
    }

    register uint64_t pixels = *(uint64_t *)&font[(c - 32) * 8];
    register uint16_t *ptr = video_get_ptr(_x, _y);

    for (register uint8_t py = 0; py < 9; py++)
    {
        for (register uint8_t px = 0; px < 7; px++)
        {
            if (pixels & 0x8000000000000000ULL)
            {
                *ptr = _fgcolor;
            }
            else if (_bgcolor != 0xAAAA)
            {
                *ptr = _bgcolor;
            }
            ptr++;
            pixels <<= 1;
        }
        ptr += 313;
    }

    _x += 6;
#if defined(VIDEO_STRICT_BOUND_CHECK)
    if (_x >= 320)
    {
        _x -= 320;
    }
#endif
}

void print_str(register const char *str)
{
    while (*str != 0)
    {
        putchar_(*str);
        str++;
    }
}

void print_xy(register const uint16_t x, register const uint16_t y, const char *str)
{
    set_xy(x, y);
    print_str(str);
}

void print_dec(register int64_t val)
{
    static uint64_t ranks[] = {0, 1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL, 100000000000ULL, 1000000000000ULL, 10000000000000ULL, 100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL, 100000000000000000ULL, 1000000000000000000ULL, 10000000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL};

    if (val < 0)
    {
        putchar_('-');
        val = -val;
    }

    register uint64_t value = (uint64_t)val;

    if ((int64_t)value < 0)
    {
        return print_str("9223372036854775808");
    }

    if (value == 0)
    {
        return putchar_('0');
    }

    register uint64_t *p = ranks;
    while (*p <= value)
    {
        p++;
    }

    register uint64_t rank = *(--p);
    do
    {
        register char c = '0';
        while (rank <= value)
        {
            value -= rank;
            c++;
        }
        putchar_(c);
        rank = *(--p);
    } while ((value != 0) || (rank != 0));
}

static inline void __attribute__((always_inline)) print_hex_char(register uint8_t val)
{
    val &= 0x0f;
    if (val >= 10)
    {
        val += 0x7;
    }
    putchar_(val + 0x30);
}

void print_hex(register uint64_t val, register uint8_t bits)
{
    print_str("0x");
    val <<= (64 - bits);
    while (bits > 0)
    {
        print_hex_char(val >> 60);
        val <<= 4;
        bits -= 4;
    }
}
