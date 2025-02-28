/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include "print.h"
#include "video.h"
#include "sound.h"
#include "draw.h"
#include "time.h"
#include "wdt.h"
#include "view.h"
#include "rand.h"
#include "ide.h"
#include "math.h"

#define DEBUG

#if defined(DEBUG)
static void render_debug(uint32_t render_time)
{
        static uint32_t render_time_max = 0;
        render_time_max = MAX(render_time, render_time_max);

        set_text_color(0x0000, 0x7FF);
        draw_box(0, 208, 150, 231);
        set_text_color(0x0000, 0xAAAA);
        print_xy(2, 210, "renderTime = ");
        print_dec(render_time);
        print_str(" us");
        print_xy(2, 222, "maxRenderTime = ");
        print_dec(render_time_max);
        print_str(" us");
}
#endif

[[noreturn]] void main(view_t *view)
{
        video_init();
        sound_init();
        ide_init();
        time_init();
        srand(ticks());

        view_switch(view);

        while (1)
        {
#if defined(DEBUG)
                uint64_t render_time = clock();
#endif
                wdt_reset();

                view_current->render(frame_counter);

#if defined(DEBUG)
                render_time = clock() - render_time;
                render_debug(render_time);
#endif
                video_vsync_wait();
                video_swap_buffers();
        }
}
