/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "detour.h"
#include "asm.h"

/*
 * The is a rudimentary implementation, you wil have to take care of not fucking up the program flow !
 * Also you have to make sure you are not stealing an instruction with a delay slot; or :explosion: !
 * You are warned, if you still decide to use it, prepare for unforseen consequences ;)
 */
void detour(uint32_t *gateway, uint32_t *src, void *dst)
{
    // Setup gateway
    gateway[0] = src[0]; // copy stolen instructions
    gateway[1] = src[1];
    gateway[2] = J(REL_ADDR(src + 2)); // jump to origin function after stolen instructions
    gateway[3] = NOP();

    // Detour
    src[0] = J(REL_ADDR(dst));
    src[1] = NOP();
}
