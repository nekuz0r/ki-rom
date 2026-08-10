/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _DEBUG_HUD_H_
#define _DEBUG_HUD_H_

#include <stdint.h>

/**
 * Folds one frame's render time into the 30s history and draws the HUD card.
 * Call once per frame, after the view has rendered and before the vsync wait.
 * The whole module compiles away unless the build defines DEBUG.
 */
void debug_hud_render(uint32_t render_us);

#endif
