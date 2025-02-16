/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wdt.h"

void wdt_reset(void)
{
    asm volatile("lw $0,%[addr]" : : [addr] "i"(&gIO.dipSwitch));
}
