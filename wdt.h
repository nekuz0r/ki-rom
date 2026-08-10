/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _WDT_H_
#define _WDT_H_

#include "io.h"

/**
 * Kicks the watchdog with no call. Wrapped in do/while(0) so it behaves as a
 * single statement, as cache.h's macros are.
 *
 * .ramcode callers need this: after the bank-select command is issued they
 * cannot call wdt_reset(), because it lives in the ROM window.
 */
#define WDT_KICK()                                                    \
    do                                                                \
    {                                                                 \
        asm volatile("lw $0,%[addr]" : : [addr] "i"(&gIO.dipSwitch)); \
    } while (0)

void wdt_reset(void);

#endif
