/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "bootselect.h"
#include "io.h"
#include "ki.h"

#define BOOTSELECT_CONFIRM_MASK (BTN_START | BTN_QP | BTN_MP | BTN_FP | BTN_QK | BTN_MK | BTN_FK)

const bootselect_entry_t bootselect_entries[BOOTSELECT_ENTRIES] = {
    [BOOTSELECT_KI1] = {
        .title = "KILLER",
        .subtitle = "INSTINCT",
        .credit = "1994  RARE LTD",
    },
    [BOOTSELECT_KI2] = {
        .title = "KILLER",
        .subtitle = "INSTINCT 2",
        .credit = "1996  RARE LTD",
    },
};

void bootselect_reset(bootselect_t *state)
{
    state->selected = BOOTSELECT_KI1;

    // Latch the buttons that are down right now instead of starting from zero.
    // The view is entered by a press that is very likely still held, and the
    // cursor moves on a released-to-pressed edge -- from zero that held button
    // would read as a fresh press and move the cursor on the first frame.
    state->previous_inputs = ~gIO.player1 & 0x7FF;

    timer_reset(&state->timer);
}

bootselect_event_t bootselect_poll(bootselect_t *state, const bool vertical)
{
    const uint16_t inputs = ~gIO.player1 & 0x7FF;
    const uint16_t pressed = inputs & ~state->previous_inputs;
    state->previous_inputs = inputs;

    const uint16_t previous = vertical ? BTN_UP : BTN_LEFT;
    const uint16_t next = vertical ? BTN_DOWN : BTN_RIGHT;

    if ((pressed & (previous | next)) != 0)
    {
        if ((pressed & next) != 0)
        {
            state->selected = (state->selected + 1) % BOOTSELECT_ENTRIES;
        }
        else
        {
            state->selected = (state->selected + BOOTSELECT_ENTRIES - 1) % BOOTSELECT_ENTRIES;
        }

        // Someone is at the panel, so the unattended-boot countdown starts over.
        timer_reset(&state->timer);
        return BOOTSELECT_MOVED;
    }

    if ((pressed & BOOTSELECT_CONFIRM_MASK) != 0)
    {
        return BOOTSELECT_CONFIRMED;
    }

    if (timer_elapsed_time(&state->timer) >= BOOTSELECT_TIMEOUT_MS)
    {
        return BOOTSELECT_EXPIRED;
    }

    return BOOTSELECT_IDLE;
}

uint32_t bootselect_remaining_ms(bootselect_t *state)
{
    const uint64_t elapsed = timer_elapsed_time(&state->timer);
    if (elapsed >= BOOTSELECT_TIMEOUT_MS)
    {
        return 0;
    }

    return BOOTSELECT_TIMEOUT_MS - (uint32_t)elapsed;
}

uint32_t bootselect_remaining_s(bootselect_t *state)
{
    // Round up, so the last displayed value is 1s and not 0s.
    return (bootselect_remaining_ms(state) + 999) / 1000;
}
