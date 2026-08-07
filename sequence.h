/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#include <stdint.h>

bool check_sequence(uint16_t *const sequence, const uint16_t timeout);

// Any table passed to check_sequence() must be 8-byte aligned: it accesses
// sequence[4..7] as a uint64_t.
extern _Alignas(8) uint16_t sequence_konami_code[];

#endif
