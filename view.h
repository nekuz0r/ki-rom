/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _VIEW_H_
#define _VIEW_H_

#include <stdint.h>

#define VIEW_MAIN 0
#define VIEW_SWITCH 1
#define VIEW_SECRET 2

typedef struct
{
  void (*render)(const uint64_t frame_count);
  void (*load)(void);
  void (*unload)(void);
} view_t;

extern view_t view_main;
extern view_t view_bootselect;
extern view_t *view_current;

void view_switch(view_t *view);

/**
 * Re-enters start.S: cache init, TLB init, .data copy, .bss clear and a fresh
 * heap, then main() with `view`. Everything SRAM holds is reinitialised, so a
 * caller does not unload the view it is leaving.
 *
 * far: callers include SRAM-resident detour hooks at 0x8000xxxx, and this lives
 * in the ROM window at 0x9FCxxxxx -- too far for jal to encode.
 */
extern void _reset(view_t *view) __attribute__((noreturn, far));

#endif
