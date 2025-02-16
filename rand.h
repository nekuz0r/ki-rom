/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _RAND_H_
#define _RAND_H_

#include <stdint.h>

void srand(uint32_t seed);
uint32_t rand(void);

#endif
