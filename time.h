/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _KI_TIME_H_
#define _KI_TIME_H_

#include <stdint.h>

void time_init(void);
const uint64_t ticks(void);
const uint64_t clock(void);
const uint64_t millis(void);
const uint64_t micros(void);

#endif
