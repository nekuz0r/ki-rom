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
#include "lzss.h"
#include "assets.h"
#include "patches.h"
#include "video.h"
#include "ki.h"
#include "roms.h"
#include "delay.h"
#include "timer.h"
#include "math.h"

static enum State {
    ST_ROM_LOAD,
    ST_ROM_PATCHES,
    ST_DONE
} state = ST_ROM_LOAD;

static image_t *logo = nullptr;
static uint8_t patch_index = 0;
static timer_t start_timer;

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

static void print_status_line(const char *description, bool status)
{
    print_str("[");
    set_text_color(status ? 0x03E0 : 0x001F, 0xAAAA);
    print_str("#");
    set_text_color(0x7FFF, 0xAAAA);
    print_str("] ");
    print_str(description);
}

static void render(const uint64_t frame_count)
{
    video_clear_framebuffer(0x0);

    draw_image(0x1E, 0x7, logo, CHROMA_KEY_NONE);
    set_text_color(0x7FFF, 0xAAAA);
    print_xy(0x5A, 0x10, KI_ROM_VERSION_STR " (" KI_BOARD_STR ")");
    print_xy(0x5A, 0x10 + 0xa, "Bootrom v2.0.0 (" __DATE__ ")");
    print_xy(0x5A, 0x10 + 0x14, "www.killer-instinct.net");

    set_text_color(0x7FFF, 0xAAAA);
    set_xy(0x1F, 0x3c);
    print_status_line("ROM decompressed @ DRAM", state > ST_ROM_LOAD);
    for (uint8_t i = 0; patches[i] != nullptr; i++)
    {
        set_xy(0x1F, 0x48 + (i * 0xc));
        print_status_line(patches[i]->name, patches[i]->status);
    }

    switch (state)
    {
    case ST_ROM_LOAD:
        zrom_load();
        state = ST_ROM_PATCHES;
        break;
    case ST_ROM_PATCHES:
        if (patches[patch_index] != nullptr)
        {
            patches[patch_index]->apply();
            patches[patch_index]->status = true;
            patch_index++;
            break;
        }
        timer_reset(&start_timer);
        state = ST_DONE;
        break;
    case ST_DONE:
        uint64_t elapsed_time = timer_elapsed_time(&start_timer);
        set_text_color(color_fade_in_out(0x1F, 0x0, FADE_SPEED_2S), 0xAAAA);
        print_xy(0x1F, 0x48 + (patch_index * 0xC) + 0xC, "Press any buttons (");
        print_dec(ceil((30000 - elapsed_time) / 1000.0f));
        print_str("s).");

        if (elapsed_time >= 30000 || is_any_input_pressed() || (gIO.dipSwitch & 0x80) == 0)
        {
            rom_start();
        }
        break;
    }
}

static void load(void)
{
    /**
     * Do not play sounds if the game is started immediatly after rom is loaded and patches applied (no wait for inputs)
     * Or the sounds are disabled by dipswitch configuration.
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

    logo = zasset_load(zasset_logo);
}
static void unload(void)
{
    free(logo);
    logo = nullptr;
}

view_t view_main = {
    .render = &render,
    .load = &load,
    .unload = &unload,
};
