/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "view.h"
#include "io.h"
#include "draw.h"
#include "time.h"
#include "print.h"
#include "sound.h"
#include "mem.h"
#include "assets.h"
#include "patches.h"
#include "video.h"
#include "ki.h"
#include "roms.h"
#include "timer.h"
#include "rand.h"

#define BOOT_TIMEOUT_MS (30000)

// --- title block
#define TITLE_X (14)
#define LOGO_X (14)
#define LOGO_Y (8)

// --- fighter. The wordmark reaches x=197 on KI2 ("INSTINCT 2" is 183px at 3x),
// so the hero starts just past that and the Rare mark takes the last column.
#define HERO_X (206)
#define HERO_Y (58)
#define GROUND_Y (164)

// --- bottom strip
#define STRIP_Y (190)
#define BAR_X0 (22)
#define BAR_X1 (298)
#define BAR_Y0 (197)
#define BAR_Y1 (207)
#define BAR_INNER (BAR_X1 - BAR_X0 - 1)
#define LABEL_Y (213)
#define FOOTER_Y (228)

#define BLACK RGB555(0, 0, 0)
#define WHITE RGB555(31, 31, 31)
#define DARK_GREY RGB555(9, 9, 11)
#define READY_GREEN RGB555(4, 29, 6)
#define BAR_EDGE RGB555(8, 8, 10)
#define BAR_TRACK RGB555(4, 4, 6)
#define BAR_TICK RGB555(2, 2, 3)

#define SHADE_RULE (4)

/**
 * KI1 is cold steel, KI2 is ember. The palette is lifted verbatim from
 * view_bootselect so the picker and the boot screen agree on what each game
 * looks like -- the hue names the game before a word of it is read.
 */
#if defined(KI)
#define KEY RGB555(10, 22, 31)
#define BG_TOP RGB555(2, 6, 14)
#define BG_BOTTOM RGB555(0, 1, 4)
#define TITLE_SUBTITLE "INSTINCT"
#define TITLE_YEAR "1994   RARE LTD"
#define CHARACTERS zasset_ki1_characters
#define CHARACTER_MIRRORED true
#else
#define KEY RGB555(31, 18, 4)
#define BG_TOP RGB555(14, 4, 2)
#define BG_BOTTOM RGB555(4, 1, 0)
#define TITLE_SUBTITLE "INSTINCT 2"
#define TITLE_YEAR "1996   RARE LTD"
#define CHARACTERS zasset_ki2_characters
#define CHARACTER_MIRRORED false
#endif

static enum State {
    ST_ROM,     // Decompressing the game image, one segment per frame.
    ST_PATCHES, // Applying patches, one per frame.
    ST_READY,   // Everything applied; counting down to the hand-off.
} state = ST_ROM;

static uint8_t rom_segment = 0;
static uint8_t patch_index = 0;
static uint8_t patch_count = 0;
static bool primed = false;
static timer_t ready_timer;

static image_t *logo = nullptr;
static image_t *hero = nullptr;

static bool is_any_input_pressed(void)
{
    static uint8_t ready = 0;
    if ((~gIO.player1 & 0x7FF) == 0 && (~gIO.player2 & 0x7FF) == 0)
    {
        ready = 1;
    }

    if (ready == 1 && ((~gIO.player1 & 0x7FF) != 0 || (~gIO.player2 & 0x7FF) != 0))
    {
        return 1;
    }

    return 0;
}

/**
 * text_width() counts the closing glyph as FONT_GLYPH_WIDTH and every other as
 * the advance, so the widths of two strings printed back to back overlap by
 * exactly one pixel.
 */
static uint16_t decimal_width(uint32_t value)
{
    uint16_t digits = 1;
    while (value >= 10)
    {
        value /= 10;
        digits++;
    }
    return (digits * 6) + 1;
}

static void print_center_value(const uint16_t cx, const uint16_t y, const char *prefix,
                               const uint32_t value, const char *suffix)
{
    const uint16_t width = text_width(prefix) + decimal_width(value) + text_width(suffix) - 2;
    set_xy(cx - (width / 2), y);
    print_str(prefix);
    print_dec(value);
    print_str(suffix);
}

static void print_center_pair(const uint16_t cx, const uint16_t y, const char *a, const char *b)
{
    const uint16_t width = text_width(a) + text_width(b) - 1;
    set_xy(cx - (width / 2), y);
    print_str(a);
    print_str(b);
}

/**
 * Patch names carry a "{PATCH} " tag that exists to group them in view_main's
 * flat list. Here the line already says PATCHING, so the tag is dropped when
 * present -- without needing every patch_t rewritten.
 */
static const char *patch_label(const char *name)
{
    const char *tag = "{PATCH} ";
    const char *rest = name;

    while (*tag != 0 && *rest == *tag)
    {
        rest++;
        tag++;
    }

    return (*tag == 0) ? rest : name;
}

static uint32_t remaining_ms(void)
{
    const uint64_t elapsed = timer_elapsed_time(&ready_timer);
    if (elapsed >= BOOT_TIMEOUT_MS)
    {
        return 0;
    }
    return BOOT_TIMEOUT_MS - (uint32_t)elapsed;
}

/** One operation per frame: three ROM segments, then one patch at a time. */
static void advance(void)
{
    switch (state)
    {
    case ST_ROM:
        zrom_load_segment(rom_segment);
        rom_segment++;
        if (rom_segment >= ZROM_SEGMENTS)
        {
            state = ST_PATCHES;
        }
        break;

    case ST_PATCHES:
        if (patches[patch_index] != nullptr)
        {
            patches[patch_index]->apply();
            patches[patch_index]->status = true;
            patch_index++;
            break;
        }
        timer_reset(&ready_timer);
        state = ST_READY;
        break;

    case ST_READY:
        break;
    }
}

static void draw_title_card(void)
{
    draw_gradient(0, 0, 319, STRIP_Y - 1, BG_TOP, BG_BOTTOM);
    draw_fill(0, STRIP_Y, 319, 239, BLACK);

    // Contact shadow: without it a 2x sprite floats on the gradient.
    for (uint16_t i = 0; i < 4; i++)
    {
        static const uint16_t widths[] = {78, 68, 54, 34};
        draw_horizontal_line(HERO_X + 40 - (widths[i] / 2), GROUND_Y + i, widths[i],
                             color_shade(BG_BOTTOM, 3 - i));
    }

    // The sprites are drawn facing right, and the hero stands to the right of
    // the wordmark, so it is mirrored to face back into the composition. Left
    // unmirrored it would face off the edge of the screen.
    if (hero != nullptr)
    {
        draw_image_ex(HERO_X, HERO_Y, hero, CHROMA_KEY_MAGENTA, &(blit_t){.zoom = 2, .mirror = CHARACTER_MIRRORED});
    }

    if (logo != nullptr)
    {
        // Keyed on black so the emblem sits on the gradient, not in a box.
        draw_image(LOGO_X, LOGO_Y, logo, 0x0000);
    }

    // 3x is the largest this font stays legible at, and it is what makes the
    // screen read as a title card rather than as a status page.
    set_text_scale(3);
    set_text_color(WHITE, 0xAAAA);
    print_xy(TITLE_X, 64, "KILLER");
    set_text_color(KEY, 0xAAAA);
    print_xy(TITLE_X, 94, TITLE_SUBTITLE);

    set_text_scale(1);
    draw_horizontal_line(TITLE_X, 128, 148, color_shade(KEY, SHADE_RULE));
    set_text_color(color_lerp(KEY, WHITE, 90), 0xAAAA);
    print_xy(TITLE_X, 136, "REVISION " KI_ROM_REVISION_STR);
    set_text_color(color_shade(KEY, SHADE_RULE), 0xAAAA);
    print_xy(TITLE_X, 148, TITLE_YEAR);
}

static void draw_bar(void)
{
    const uint8_t steps = ZROM_SEGMENTS + patch_count;
    const bool ready = (state == ST_READY);

    uint32_t filled;
    uint16_t color;

    if (ready)
    {
        // The same bar now measures the auto-boot window instead of the work.
        filled = (BAR_INNER * remaining_ms()) / BOOT_TIMEOUT_MS;
        color = READY_GREEN;
    }
    else
    {
        filled = ((uint32_t)BAR_INNER * (rom_segment + patch_index)) / steps;
        color = KEY;
    }

    set_text_color(BAR_EDGE, 0xAAAA);
    draw_box(BAR_X0, BAR_Y0, BAR_X1, BAR_Y1);
    draw_fill(BAR_X0 + 1, BAR_Y0 + 1, BAR_X1 - 1, BAR_Y1 - 1, BAR_TRACK);

    if (filled > 0)
    {
        draw_horizontal_line(BAR_X0 + 1, BAR_Y0 + 1, filled, color_lerp(color, WHITE, 110));
        for (uint16_t y = BAR_Y0 + 2; y < BAR_Y1; y++)
        {
            draw_horizontal_line(BAR_X0 + 1, y, filled, color);
        }
    }

    // One cell per operation, derived from the patch table rather than fixed:
    // the table is built by the preprocessor and runs from one patch (ki-p47)
    // to five (ki-l15d on A-20351). Dropped once the bar counts seconds, where
    // a boot-step scale would be measuring the wrong thing.
    if (!ready)
    {
        for (uint8_t t = 1; t < steps; t++)
        {
            const uint16_t tx = BAR_X0 + 1 + (uint16_t)(((uint32_t)BAR_INNER * t) / steps);
            draw_fill(tx, BAR_Y0 + 1, tx, BAR_Y1 - 1, BAR_TICK);
        }
    }
}

static void draw_strip(void)
{
    draw_horizontal_line(0, STRIP_Y, 320, color_shade(KEY, SHADE_RULE));
    draw_bar();

    set_text_scale(1);
    switch (state)
    {
    case ST_ROM:
        set_text_color(WHITE, 0xAAAA);
        print_center_value(160, LABEL_Y, "LOADING GAME ROM   ", rom_segment + 1u, " OF 3");
        break;

    case ST_PATCHES:
        set_text_color(WHITE, 0xAAAA);
        if (patches[patch_index] != nullptr)
        {
            print_center_pair(160, LABEL_Y, "PATCHING   ", patch_label(patches[patch_index]->name));
        }
        else
        {
            // The single frame between the last patch and the countdown.
            print_center(160, LABEL_Y, "READY");
        }
        break;

    case ST_READY:
        set_text_color(color_fade_in_out(WHITE, RGB555(12, 12, 14), FADE_SPEED_2S), 0xAAAA);
        print_center_value(160, LABEL_Y, "PRESS ANY BUTTON TO START   (",
                           (remaining_ms() + 999) / 1000, "s)");
        break;
    }

    set_text_color(DARK_GREY, 0xAAAA);
    print_center(160, FOOTER_Y, "BOOTROM V2.0.1   " KI_BOARD_STR "   WWW.KILLER-INSTINCT.NET");
}

static void render(const uint64_t frame_count)
{
    (void)frame_count;

    /**
     * The work runs before the draw, and the first call does no work at all.
     * Both matter. main() swaps buffers only after render() returns, so a frame
     * drawn before its step would sit on screen naming the step that has
     * already finished; and doing work on the first call would spend the first
     * multi-second decompression showing the green fill start.S left behind.
     */
    if (primed)
    {
        advance();
    }
    else
    {
        primed = true;
    }

    video_clear_framebuffer(0x0);
    draw_title_card();
    draw_strip();

    if (state == ST_READY)
    {
        if (remaining_ms() == 0 || is_any_input_pressed() || (gIO.dipSwitch & 0x80) == 0)
        {
            rom_start();
        }
    }
}

static void load(void)
{
    /**
     * Do not play sounds if the game is started immediatly after rom is loaded
     * and patches applied (no wait for inputs), or the sounds are disabled by
     * dipswitch configuration.
     */
    if ((gIO.dipSwitch & 0x80) != 0 && (gIO.dipSwitch & 0x40) != 0)
    {
#if defined(KI)
        sound_play(0x23);  // Theme music
        sound_play(0x550); // Killer Instinct
#elif defined(KI2)
        sound_play(0x1771); // Theme music
        sound_play(0x551);  // Killer Instinct 2
#endif
    }

    state = ST_ROM;
    rom_segment = 0;
    patch_index = 0;
    primed = false;

    patch_count = 0;
    while (patches[patch_count] != nullptr)
    {
        patch_count++;
    }

    logo = zasset_load(zasset_logo);

    const uint8_t pick = rand() % ZASSET_CHARACTERS;
    hero = zasset_load(CHARACTERS[pick]);
}

static void unload(void)
{
    free(logo);
    free(hero);
    logo = nullptr;
    hero = nullptr;
}

view_t view_main = {
    .render = &render,
    .load = &load,
    .unload = &unload,
};
