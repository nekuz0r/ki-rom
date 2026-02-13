/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _SOUND_H_
#define _SOUND_H_

#include <stdint.h>

void sound_set_volume(uint8_t level);
void sound_play(uint16_t track);
void sound_init(void);

#endif
