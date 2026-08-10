/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "bootrom.h"
#include "ramcode.h"

RAMCODE_FN bool bootrom_swap(const uint8_t bank)
{
    (void)bank;

    // Task 3 fills this in. Reporting failure is the safe answer: the caller
    // boots the image that is already mapped.
    return false;
}
