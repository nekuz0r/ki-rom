/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include "video.h"
#include "sound.h"
#include "time.h"
#include "wdt.h"
#include "view.h"
#include "rand.h"
#include "ide.h"

#if defined(DEBUG)
#include "debug_hud.h"
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
                uint64_t render_start = clock();
#endif
                wdt_reset();

                view_current->render(frame_counter);

#if defined(DEBUG)
                debug_hud_render((uint32_t)(clock() - render_start));
#endif
                video_vsync_wait();
                video_swap_buffers();
        }
}
