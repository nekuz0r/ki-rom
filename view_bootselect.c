/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "view.h"
#include "print.h"
#include "video.h"

static void render(const uint8_t frame_count)
{
    video_clear_framebuffer(0x7FFF7FFF7FFF7FFF);

    set_text_color(0x0, 0xAAAA);
    print_xy(0x5A, 0x10, "Multiboot (WIP)");
}

static void load(void)
{
}

static void unload(void)
{
}

view_t view_bootselect = {
    .render = &render,
    .load = &load,
    .unload = &unload,
};
