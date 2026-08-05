/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "sequence.h"
#include "io.h"
#include "time.h"

uint16_t sequence_konami_code[] = {0x0000, 0xDEAD, 0xDEAD, 0xDEAD, 0x0000, 0x0000, 0x0000, 0x0000, BTN_UP, BTN_UP, BTN_DOWN, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_LEFT, BTN_RIGHT, BTN_QP, BTN_QK, 0xFFFF};

bool check_sequence(uint16_t *const sequence, const uint16_t timeout)
{
    static uint16_t prevP1Inputs = 0x7FF;
    uint16_t p1Inputs = gIO.player1 & 0x7FF;
    uint64_t now = millis();

    // Layout: [0] index, [1..3] padding, [4..7] 64-bit timestamp, [8..] inputs.
    uint16_t *index = sequence;
    uint64_t *time = (uint64_t *)(sequence + 4);
    uint16_t *inputs = sequence + 8;

    if (*time != 0 && (now - *time) >= timeout)
    {
        *index = 0;
        *time = 0;
    }

    if (p1Inputs != prevP1Inputs && p1Inputs != 0x7FF)
    {
        if ((p1Inputs & inputs[*index]) == 0)
        {
            *index += 1;
            *time = now;
        }
        else
        {
            *index = 0;
            *time = 0;
        }
    }
    prevP1Inputs = p1Inputs;

    if (inputs[*index] == 0xFFFF)
    {
        *index = 0;
        *time = 0;
        return true;
    }

    return false;
}
