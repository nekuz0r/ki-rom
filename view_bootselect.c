/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "view.h"
#include "bootselect.h"
#include "print.h"
#include "video.h"
#include "assets.h"
#include "draw.h"
#include "rand.h"
#include "io.h"
#include "ki.h"
#include "mem.h"
#include "bootrom.h"

#define HUD_Y (202) // First scanline of the bottom bar; the arena is above it.

#define BLACK RGB555(0, 0, 0)
#define WHITE RGB555(31, 31, 31)
#define GREY RGB555(16, 16, 17)
#define DARK_GREY RGB555(9, 9, 11)
#define SEAM RGB555(13, 13, 16)
#define SEAM_EDGE RGB555(5, 5, 7)
#define TRACK RGB555(5, 5, 7)

// color_shade() works in eighths. 3/8 is dark enough to kill the unselected
// side without turning it into an unreadable black rectangle.
#define SHADE_UNSELECTED (3)
#define SHADE_GHOST (2)
#define SHADE_GHOST_UNSELECTED (1)

typedef struct
{
    uint16_t key;    // Accent: frame, title and countdown all take it.
    uint16_t top;    // Backdrop gradient.
    uint16_t bottom; //
    uint16_t centre; // Where the hero and the title are centred.
} panel_style_t;

static const panel_style_t styles[BOOTSELECT_ENTRIES] = {
    // KI1 is cold steel, KI2 is ember: the two are told apart by hue before a
    // single word is read.
    [BOOTSELECT_KI1] = {
        .key = RGB555(10, 22, 31),
        .top = RGB555(2, 6, 14),
        .bottom = RGB555(0, 1, 4),
        .centre = 82,
    },
    [BOOTSELECT_KI2] = {
        .key = RGB555(31, 18, 4),
        .top = RGB555(14, 4, 2),
        .bottom = RGB555(4, 1, 0),
        .centre = 240,
    },
};

static bootselect_t state;
static image_t *heroes[BOOTSELECT_ENTRIES] = {nullptr, nullptr};
static const char *hero_names[BOOTSELECT_ENTRIES] = {nullptr, nullptr};

/**
 * The divider leans: 172 at the top scanline, 148 at the bottom of the arena. A
 * vertical split reads as two windows; a slanted one reads as one screen.
 */
static inline uint16_t seam_x(const uint16_t y)
{
    return 172 - ((y * 24) / HUD_Y);
}

static void draw_backdrop(const uint8_t entry, const bool selected)
{
    const panel_style_t *const style = &styles[entry];

    for (uint16_t y = 0; y < HUD_Y; y++)
    {
        const uint16_t seam = seam_x(y);
        uint16_t color = color_lerp(style->top, style->bottom, (uint8_t)(((uint32_t)y * 255) / HUD_Y));

        if (!selected)
        {
            color = color_shade(color, SHADE_UNSELECTED);
        }

        if (entry == BOOTSELECT_KI1)
        {
            draw_horizontal_line(0, y, seam - 2, color);
        }
        else
        {
            draw_horizontal_line(seam + 3, y, 320 - (seam + 3), color);
        }
    }
}

/**
 * Four corner brackets rather than a closed rectangle: a full frame at this size
 * fights the title for attention, and the open corners still read as a target.
 */
static void draw_brackets(const uint16_t cx, const uint16_t color)
{
    const uint16_t x0 = cx - 64;
    const uint16_t x1 = cx + 64;
    const uint16_t y0 = 6;
    const uint16_t y1 = 194;

    draw_fill(x0, y0, x0 + 29, y0 + 2, color);
    draw_fill(x1 - 29, y0, x1, y0 + 2, color);
    draw_fill(x0, y1 - 2, x0 + 29, y1, color);
    draw_fill(x1 - 29, y1 - 2, x1, y1, color);

    draw_fill(x0, y0, x0 + 2, y0 + 25, color);
    draw_fill(x1 - 2, y0, x1, y0 + 25, color);
    draw_fill(x0, y1 - 25, x0 + 2, y1, color);
    draw_fill(x1 - 2, y1 - 25, x1, y1, color);
}

static void draw_panel(const uint8_t entry, const bool selected)
{
    const panel_style_t *const style = &styles[entry];
    const bootselect_entry_t *const meta = &bootselect_entries[entry];
    const uint16_t cx = style->centre;
    const uint16_t accent = selected ? style->key : color_shade(style->key, SHADE_UNSELECTED);

    draw_backdrop(entry, selected);

    // Contact shadow: without it a 2x sprite floats on the gradient.
    for (uint16_t i = 0; i < 4; i++)
    {
        static const uint16_t widths[] = {64, 56, 44, 28};
        draw_horizontal_line(cx - (widths[i] / 2), 172 + i, widths[i], color_shade(style->bottom, 3 - i));
    }

    if (heroes[entry] != nullptr)
    {
        draw_image_ex(cx - 40, 66, heroes[entry], CHROMA_KEY_MAGENTA,
                      &(blit_t){
                          .zoom = 2,
                          .mirror = false,
                          .shade = selected ? 0 : SHADE_UNSELECTED,
                      });
    }

    set_text_scale(2);
    set_text_color(selected ? WHITE : GREY, 0xAAAA);
    print_center(cx, 14, meta->title);
    set_text_color(selected ? accent : DARK_GREY, 0xAAAA);
    print_center(cx, 32, meta->subtitle);

    set_text_scale(1);
    set_text_color(selected ? accent : DARK_GREY, 0xAAAA);
    print_center(cx, 52, meta->credit);

    if (hero_names[entry] != nullptr)
    {
        set_text_color(selected ? WHITE : GREY, 0xAAAA);
        print_center(cx, 180, hero_names[entry]);
    }

    if (selected)
    {
        draw_brackets(cx, color_fade_in_out(style->key, WHITE, FADE_SPEED_2S));
    }
}

static void draw_seam(void)
{
    for (uint16_t y = 0; y < HUD_Y; y++)
    {
        const uint16_t seam = seam_x(y);
        draw_horizontal_line(seam - 3, y, 7, BLACK);
        draw_horizontal_line(seam - 1, y, 1, SEAM_EDGE);
        draw_horizontal_line(seam, y, 1, SEAM);
        draw_horizontal_line(seam + 1, y, 1, SEAM_EDGE);
    }
}

static void draw_hud(const uint16_t key)
{
    draw_fill(0, HUD_Y, 319, 239, BLACK);
    draw_horizontal_line(0, HUD_Y, 320, color_shade(key, 4));

    set_text_scale(1);
    set_text_color(color_fade_in_out(key, RGB555(11, 11, 13), FADE_SPEED_2S), 0xAAAA);
    print_center(160, 207, "<  SELECT  >     START = BOOT");

    // Countdown as a draining bar: readable at a glance from the cabinet glass,
    // where a two-digit number is not.
    const uint16_t left = 62;
    const uint16_t width = 196;
    const uint32_t filled = (width * bootselect_remaining_ms(&state)) / BOOTSELECT_TIMEOUT_MS;
    draw_fill(left, 221, left + width - 1, 223, TRACK);
    if (filled > 0)
    {
        draw_horizontal_line(left, 221, filled, color_lerp(key, WHITE, 110));
        draw_horizontal_line(left, 222, filled, key);
        draw_horizontal_line(left, 223, filled, key);
    }

    set_text_color(DARK_GREY, 0xAAAA);
    print_center(160, 228, "BOOTROM V2.1.0   " KI_BOARD_STR "   WWW.KILLER-INSTINCT.NET");
}

static const uint8_t banks[BOOTSELECT_ENTRIES] = {
    [BOOTSELECT_KI1] = BOOTROM_BANK_KI1,
    [BOOTSELECT_KI2] = BOOTROM_BANK_KI2,
};

/**
 * Hands off to the chosen game. Never returns.
 *
 * Selecting the bank that is already mapped issues no IDE command at all --
 * which is also the guard against swapping this image out from under itself.
 * The other bank goes through bootrom_swap(), which enters the newly mapped
 * image through its own reset vector and does not come back.
 *
 * _reset rather than view_switch: it reinitialises everything SRAM holds, so
 * the hero images this view allocated are reclaimed wholesale and unload() is
 * never needed. Same hand-off patch_kix_reset.c makes to re-enter this view.
 */
[[noreturn]] static void boot(const uint8_t entry)
{
    const uint8_t bank = banks[entry];

    if (bank != BOOTROM_BANK_SELF)
    {
        // Returns only when the bank is unchanged under the hardware model
        // bootrom.c documents -- the device never went ready and no command
        // was issued, or it answered in a way that means it never switched.
        // Either way the mapped bank is still ours and falling through is
        // safe.
        bootrom_swap(bank);
    }

    _reset_boot();
}

static void render(const uint64_t frame_count)
{
    (void)frame_count;

    // Unselected first: the selected side's brackets are allowed to sit over the
    // seam, so it has to be painted last.
    const uint8_t selected = state.selected;
    draw_panel(selected ^ 1, false);
    draw_panel(selected, true);
    draw_seam();
    draw_hud(styles[selected].key);

    switch (bootselect_poll(&state, false))
    {
    case BOOTSELECT_CONFIRMED:
    case BOOTSELECT_EXPIRED:
        return boot(state.selected);
    default:
        break;
    }
}

static void load_pair(const uint8_t entry, uint8_t *const characters[], const char *const names[])
{
    const uint8_t hero = rand() % ZASSET_CHARACTERS;

    heroes[entry] = zasset_load(characters[hero]);
    hero_names[entry] = names[hero];
}

static void load(void)
{
    bootselect_reset(&state);

    // bootselect_reset() always highlights KI1 -- correct for a soft-reset
    // combo, which used to just re-enter this view. Now BOOTSELECT_EXPIRED
    // issues an IDE command, so a KI2 cabinet whose operator triggers the
    // combo and walks away would come back running KI1. Re-point the
    // highlight at this image's own bank so an unattended timeout is a true
    // no-op: same bank, no IDE command at all.
    for (uint8_t entry = 0; entry < BOOTSELECT_ENTRIES; entry++)
    {
        if (banks[entry] == BOOTROM_BANK_SELF)
        {
            state.selected = entry;
            break;
        }
    }

    load_pair(BOOTSELECT_KI1, zasset_ki1_characters, zasset_ki1_character_names);
    load_pair(BOOTSELECT_KI2, zasset_ki2_characters, zasset_ki2_character_names);
}

static void unload(void)
{
    for (uint8_t i = 0; i < BOOTSELECT_ENTRIES; i++)
    {
        free(heroes[i]);
        heroes[i] = nullptr;
        hero_names[i] = nullptr;
    }
}

view_t view_bootselect = {
    .render = &render,
    .load = &load,
    .unload = &unload,
};
