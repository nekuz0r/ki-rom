/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <umm_malloc/umm_malloc.h>
#include "view.h"
#include "print.h"
#include "video.h"
#include "assets.h"
#include "draw.h"
#include "rand.h"
#include "io.h"
#include "math.h"

static image_t *ki1_character = nullptr;
static image_t *ki2_character = nullptr;

static void render(const uint64_t frame_count)
{

    video_clear_framebuffer(0);

    set_text_color(0x7FF, 0xAAAA);
    print_xy(0x5A, 0x10, "Multiboot (WIP)");

    // zasset_load() returns NULL when the heap cannot satisfy the allocation.
    if (!ki1_character || !ki2_character)
    {
        return;
    }

    uint16_t color = color_fade_in_out(0x7FF, 0x1F, FADE_SPEED_3S);
    set_text_color(color, color);
    draw_box(ki1_character->width - 5, 0x7, (ki1_character->width * 2) + 5, 0x7 + (ki1_character->height) + 5);

    static bool mirrored = false;
    if ((frame_count % 60) == 0)
    {
        mirrored = !mirrored;
    }

    if (mirrored)
    {
        draw_image_mirror_x(ki1_character->width, 0x7, ki1_character, 0x7c1f);
    }
    else
    {
        draw_image(ki1_character->width, 0x7, ki1_character, 0x7c1f);
    }
    // draw_image(ki1_character->width, 0x7, ki1_character, 0x7c1f);
    draw_image(320 - (ki2_character->width * 2), 0x7, ki2_character, 0x7c1f);

    if ((~gIO.player1 & 0x7FF) == BTN_RIGHT)
    {
        return view_switch(&view_main);
    }
}

static void load(void)
{
    ki1_character = zasset_load(zasset_ki1_characters[rand() % 11]);
    ki2_character = zasset_load(zasset_ki2_characters[rand() % 11]);
}

static void unload(void)
{
    free(ki1_character);
    free(ki2_character);
    ki1_character = nullptr;
    ki2_character = nullptr;
}

view_t view_bootselect = {
    .render = &render,
    .load = &load,
    .unload = &unload,
};
