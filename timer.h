/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _TIMER_H_
#define _TIMER_H_

#include "time.h"

typedef struct
{
  uint64_t time;
} timer_t;

void timer_reset(timer_t *timer);
uint64_t timer_elapsed_time(timer_t *timer);

#endif
