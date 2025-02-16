/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _IO_H_
#define _IO_H_

#include <stddef.h>
#include <stdint.h>

#if defined(KI_BOARD_19489)
typedef struct
{
    _Alignas(8) union
    {
        uint16_t player1;     // offset = 0x80 (r)
        uint16_t vramControl; // offset = 0x80 (w)
    };
    _Alignas(8) union
    {
        uint16_t player2;    // offset = 0x88 (r)
        uint16_t soundReset; // offset = 0x88 (w)
    };
    _Alignas(8) union
    {
        uint16_t volume;       // offset = 0x90 (r)
        uint16_t soundControl; // offset = 0x90 (w)
    };
    _Alignas(8) uint8_t soundData; // offset = 0x98 (w)
    _Alignas(8) uint8_t dipSwitch; // offset = 0xa0 (r)
    _Alignas(8) uint8_t _unused0;
    _Alignas(8) uint8_t coinControl; // offset = 0xb0 (w)
    _Alignas(8) uint8_t _unused1;
} gpio_t;

static_assert(0x80 + offsetof(gpio_t, player1) == 0x80);
static_assert(0x80 + offsetof(gpio_t, player2) == 0x88);
static_assert(0x80 + offsetof(gpio_t, volume) == 0x90);
static_assert(0x80 + offsetof(gpio_t, soundData) == 0x98);
static_assert(0x80 + offsetof(gpio_t, dipSwitch) == 0xa0);
static_assert(0x80 + offsetof(gpio_t, coinControl) == 0xb0);

#elif defined(KI_BOARD_20351)
typedef struct
{
    _Alignas(8) union
    {
        uint16_t volume;       // offset = 0x80 (r)
        uint16_t soundControl; // offset = 0x80 (w)
    };
    _Alignas(8) uint8_t dipSwitch; // offset = 0x88 (r)
    _Alignas(8) union
    {
        uint16_t player2;    // offset = 0x90 (r)
        uint16_t soundReset; // offset = 0x90 (w)
    };
    _Alignas(8) union
    {
        uint16_t player1;     // offset = 0x98 (r)
        uint16_t vramControl; // offset = 0x98 (w)
    };
    _Alignas(8) uint8_t soundData; // offset = 0xa0 (w)
    _Alignas(8) uint8_t _unused0;
    _Alignas(8) uint8_t _unused1;
    _Alignas(8) uint8_t coinControl; // offset = 0xb8 (w)
} gpio_t;

static_assert(0x80 + offsetof(gpio_t, player1) == 0x98);
static_assert(0x80 + offsetof(gpio_t, player2) == 0x90);
static_assert(0x80 + offsetof(gpio_t, volume) == 0x80);
static_assert(0x80 + offsetof(gpio_t, soundData) == 0xa0);
static_assert(0x80 + offsetof(gpio_t, dipSwitch) == 0x88);
static_assert(0x80 + offsetof(gpio_t, coinControl) == 0xb8);

#endif

extern volatile gpio_t gIO;

#define BTN_QP 0x01
#define BTN_MP 0x02
#define BTN_FP 0x04
#define BTN_QK 0x08
#define BTN_MK 0x10
#define BTN_FK 0x20
#define BTN_UP 0x40
#define BTN_DOWN 0x80
#define BTN_LEFT 0x100
#define BTN_RIGHT 0x200
#define BTN_START 0x400

#endif
