/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _BOOTSELECT_H_
#define _BOOTSELECT_H_

#include <stdint.h>
#include "timer.h"

#define BOOTSELECT_KI1 (0)
#define BOOTSELECT_KI2 (1)
#define BOOTSELECT_ENTRIES (2)
#define BOOTSELECT_TIMEOUT_MS (30000)

typedef enum
{
    BOOTSELECT_IDLE = 0,
    BOOTSELECT_MOVED,     // The highlight changed; reload whatever depends on it.
    BOOTSELECT_CONFIRMED, // The operator asked for the highlighted entry.
    BOOTSELECT_EXPIRED,   // Nobody chose in time; boot the highlighted entry.
} bootselect_event_t;

typedef struct
{
    uint8_t selected;
    uint16_t previous_inputs;
    timer_t timer;
} bootselect_t;

typedef struct
{
    const char *title;    // "KILLER"
    const char *subtitle; // "INSTINCT" / "INSTINCT 2"
    const char *credit;   // "1994  RARE LTD" / "1996  RARE LTD"
} bootselect_entry_t;

extern const bootselect_entry_t bootselect_entries[BOOTSELECT_ENTRIES];

void bootselect_reset(bootselect_t *state);

/**
 * Call once per frame. `vertical` picks which axis walks the list: up/down for a
 * stacked list, left/right for side-by-side panels.
 */
bootselect_event_t bootselect_poll(bootselect_t *state, const bool vertical);

uint32_t bootselect_remaining_ms(bootselect_t *state);
uint32_t bootselect_remaining_s(bootselect_t *state);

#endif
