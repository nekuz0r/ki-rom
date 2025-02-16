/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "timer.h"

void timer_reset(timer_t *timer)
{
    timer->time = millis();
}

uint64_t timer_elapsed_time(timer_t *timer)
{
    return millis() - timer->time;
}
