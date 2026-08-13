/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "debug_hud.h"

#if defined(DEBUG)

#include "draw.h"
#include "print.h"
#include "time.h"
#include "math.h"

// 60 buckets of 500ms is exactly the 30s the card plots, at one bucket per pixel
// column. Driving the advance off millis() rather than a frame count keeps the
// window 30s wide whatever the refresh rate turns out to be.
#define HUD_BUCKETS (60)
#define HUD_BUCKET_MS (500)

// A 500ms window quantises to +/-2fps; a second is stable enough to read.
#define HUD_FPS_WINDOW_MS (1000)

// The card. Clear of view_main's logo (x=14) and hero art (HERO_Y 58), and held
// 13px off the right edge so an overscanning monitor does not eat it.
#define HUD_X0 (241)
#define HUD_Y0 (6)
#define HUD_X1 (306)
#define HUD_Y1 (44)

// Content sits two pixels inside the border, which lands the plot on exactly
// HUD_BUCKETS pixels.
#define HUD_IN_X0 (244)
#define HUD_IN_X1 (303)

#define HUD_ROW_FPS_Y (9)
#define HUD_BUDGET_Y (19)
#define HUD_PLOT_Y0 (20)
#define HUD_PLOT_Y1 (31)
#define HUD_PLOT_H (HUD_PLOT_Y1 - HUD_PLOT_Y0 + 1)
#define HUD_BASE_Y (32)
#define HUD_ROW_RANGE_Y (34)

// The vsync budget is the top of the plot, so a bar height always means the same
// fraction of the frame deadline rather than a fraction of whatever the worst
// sample happened to be.
#define HUD_BUDGET_US (16667)
#define HUD_WARN_US (10000)

#define HUD_FPS_GOOD (50)
#define HUD_FPS_WARN (25)

// The card interior is a scrim, not a fill: it darkens whatever the view drew there
// rather than painting over it. 1 == half brightness, 2 == a quarter, 3 == an eighth.
// A quarter is the balance point: the screen behind the card still reads, and the
// bars and both text rows still sit on something close enough to black to be crisp.
#define HUD_SCRIM_SHIFT (2)

// The chrome has to read against the scrim rather than against black. At a quarter
// the backdrop tops out around (7,7,7), so the values these started at (5/7/9) would
// sink into bright content -- lifted just clear of that ceiling, which keeps the
// card's dark character while preserving the ordering and the slight blue cast.
#define HUD_BORDER RGB555(9, 9, 12)
#define HUD_GRID RGB555(12, 12, 15)
#define HUD_LABEL RGB555(16, 16, 19)
#define HUD_VALUE RGB555(31, 31, 31)
#define HUD_GOOD RGB555(4, 29, 6)
#define HUD_WARN RGB555(31, 24, 4)
#define HUD_OVER RGB555(31, 6, 6)

// print.c treats this background as "write foreground pixels only".
#define HUD_TRANSPARENT (0xAAAA)

// hi[i] == 0 marks a bucket nothing has landed in yet. A render cannot honestly
// measure 0us, so samples are floored at 1 and the sentinel stays unambiguous.
// Storing lo as well as hi is what makes the min readout a true minimum instead
// of a minimum of per-bucket maxima.
static uint16_t hi[HUD_BUCKETS];
static uint16_t lo[HUD_BUCKETS];
static uint8_t head = 0;

static bool started = false;
static uint64_t bucket_start_ms = 0;

static uint32_t fps_frames = 0;
static uint64_t fps_window_ms = 0;
static uint16_t fps = 0;

static uint16_t hud_color(uint32_t us)
{
    if (us >= HUD_BUDGET_US)
    {
        return HUD_OVER;
    }

    if (us >= HUD_WARN_US)
    {
        return HUD_WARN;
    }

    return HUD_GOOD;
}

// One decimal below 10ms, where the tenths are what you are tuning against, and
// whole milliseconds at or above it, where only the magnitude is. That caps the
// range row at 9 characters -- "9.9-9.9ms", 55px -- inside the 60px content box.
// Tenths throughout let "65.5-65.5ms" reach 67px and spill past the card border.
static void print_ms(uint32_t us)
{
    print_dec(us / 1000);

    if (us < 10000)
    {
        print_str(".");
        print_dec((us % 1000) / 100);
    }
}

static void hud_sample(uint32_t render_us, uint64_t now_ms)
{
    // now_ms - bucket_start_ms is unsigned, so a clock that ever went backwards
    // would underflow it and spin the advance loop below ~2^55 times with no
    // wdt_reset() in reach. Not reachable today: ticks() is monotone and a reset
    // re-clears .bss before this runs. Nothing states either as a contract, so
    // re-anchor instead of trusting them.
    if (started && (now_ms < bucket_start_ms))
    {
        started = false;
    }

    if (!started)
    {
        // millis() is not zero-based -- time_init() seeds the accumulator from
        // whatever CP0 Count already held -- so the first call has to anchor both
        // windows, or the loop below retires all 60 buckets at once.
        started = true;
        bucket_start_ms = now_ms;
        fps_window_ms = now_ms;
    }

    // A stall spanning several buckets has to retire every one it covered, or the
    // time axis silently compresses. Hence the loop rather than an if.
    while ((now_ms - bucket_start_ms) >= HUD_BUCKET_MS)
    {
        head = (head + 1) % HUD_BUCKETS;
        // Only hi is cleared. The seeding branch below writes both bounds the
        // first time a bucket is touched, so a stale lo is never read, and
        // clearing it would need a sentinel of its own.
        hi[head] = 0;
        bucket_start_ms += HUD_BUCKET_MS;
    }

    const uint16_t us = (uint16_t)MIN(MAX(render_us, 1u), 65535u);
    if (hi[head] == 0)
    {
        hi[head] = us;
        lo[head] = us;
    }
    else
    {
        hi[head] = MAX(hi[head], us);
        lo[head] = MIN(lo[head], us);
    }

    fps_frames++;
    const uint64_t elapsed = now_ms - fps_window_ms;
    if (elapsed >= HUD_FPS_WINDOW_MS)
    {
        // Rounded, not truncated: 60 frames across 1002ms is 60fps, not 59.
        fps = (uint16_t)(((fps_frames * 1000ull) + (elapsed / 2)) / elapsed);
        fps_frames = 0;
        fps_window_ms = now_ms;
    }
}

static void hud_draw_card(void)
{
    // A scrim rather than a fill. main.c runs the HUD after view_current->render(),
    // so the pixels under the card are final and darkening them leaves the screen
    // behind it visible -- which is the point on view_bootselect, where there is no
    // free corner and the card has to sit over the KI2 panel. Reading the frame
    // buffer back is only safe because both views clear it at the top of every
    // render(); a view that drew incrementally would see the scrim compound frame
    // over frame until the card went black. The rectangle is exactly the one
    // draw_box would otherwise have filled.
    draw_fill_darken(HUD_X0 + 1, HUD_Y0 + 1, HUD_X1 - 1, HUD_Y1 - 1, HUD_SCRIM_SHIFT);

    // draw_box takes its edge colour from the text foreground and its fill from the
    // text background, so one set_text_color does both. HUD_TRANSPARENT is what
    // makes it skip the fill and leave the scrim standing.
    set_text_color(HUD_BORDER, HUD_TRANSPARENT);
    draw_box(HUD_X0, HUD_Y0, HUD_X1, HUD_Y1);
}

static void hud_draw_fps(void)
{
    const uint16_t color = (fps >= HUD_FPS_GOOD)   ? HUD_GOOD
                           : (fps >= HUD_FPS_WARN) ? HUD_WARN
                                                   : HUD_OVER;

    set_text_color(color, HUD_TRANSPARENT);
    set_xy(HUD_IN_X0, HUD_ROW_FPS_Y);
    print_dec(fps);

    set_text_color(HUD_LABEL, HUD_TRANSPARENT);
    print_str(" FPS");
}

static void hud_draw_plot(void)
{
    // Dashed, because the top of the plot is the frame deadline rather than a
    // gridline: a bar touching it has used the whole budget.
    for (uint16_t x = HUD_IN_X0; x <= HUD_IN_X1; x += 2)
    {
        draw_fill(x, HUD_BUDGET_Y, x, HUD_BUDGET_Y, HUD_GRID);
    }

    draw_horizontal_line(HUD_IN_X0, HUD_BASE_Y, HUD_IN_X1 - HUD_IN_X0 + 1, HUD_BORDER);

    for (uint8_t i = 0; i < HUD_BUCKETS; i++)
    {
        // Oldest on the left. head -- the bucket still filling -- is the
        // rightmost column, so the newest bar moves every frame rather than
        // twice a second.
        const uint16_t us = hi[(head + 1 + i) % HUD_BUCKETS];
        if (us == 0)
        {
            continue;
        }

        // Fixed scale against the budget, so a height means the same thing on
        // every frame. Anything over the deadline clamps to full height and is
        // already red by way of hud_color().
        uint16_t h = (uint16_t)(((uint32_t)MIN(us, HUD_BUDGET_US) * HUD_PLOT_H) / HUD_BUDGET_US);
        h = MAX(h, 1);

        draw_fill(HUD_IN_X0 + i, HUD_PLOT_Y1 - h + 1, HUD_IN_X0 + i, HUD_PLOT_Y1, hud_color(us));
    }
}

static void hud_draw_range(void)
{
    uint16_t vmin = 0xFFFF;
    uint16_t vmax = 0;

    for (uint8_t i = 0; i < HUD_BUCKETS; i++)
    {
        if (hi[i] == 0)
        {
            continue;
        }

        vmin = MIN(vmin, lo[i]);
        vmax = MAX(vmax, hi[i]);
    }

    set_xy(HUD_IN_X0, HUD_ROW_RANGE_Y);

    if (vmax == 0)
    {
        set_text_color(HUD_LABEL, HUD_TRANSPARENT);
        print_str("--");
        return;
    }

    set_text_color(HUD_VALUE, HUD_TRANSPARENT);
    print_ms(vmin);

    set_text_color(HUD_LABEL, HUD_TRANSPARENT);
    print_str("-");

    set_text_color(hud_color(vmax), HUD_TRANSPARENT);
    print_ms(vmax);

    set_text_color(HUD_LABEL, HUD_TRANSPARENT);
    print_str("ms");
}

void debug_hud_render(uint32_t render_us)
{
    hud_sample(render_us, millis());

    // print.c's scale is a sticky global. Every width on this card is computed for
    // scale 1, and putchar_'s scaled path does not clip, so inheriting a scale that
    // a view happened to leave behind would run the range row off the screen.
    set_text_scale(1);

    hud_draw_card();
    hud_draw_fps();
    hud_draw_plot();
    hud_draw_range();
}

#endif
