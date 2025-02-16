/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _DETOUR_H_
#define _DETOUR_H_

#include <stdint.h>

#define STR(x) #x
#define XSTR(x) STR(x)

#define DETOUR_GATEWAY(name) \
    __attribute__((section(".detour.gateway"))) static uint32_t name##_gateway[4] = {0};

#define DETOUR_FN __attribute__((noinline, section(".detour.hook")))

#define DETOUR(name, src, dst) detour(name##_gateway, (void *)(src), (void *)(dst))

#define DETOUR_RETURN(name, reg)                                                       \
    asm volatile("la $" XSTR(reg) ",%[gateway]\n" : : [gateway] "i"(&name##_gateway)); \
    asm volatile("jr $" XSTR(reg) "\n");                                               \
    asm volatile("nop\n")

void detour(uint32_t *gateway, uint32_t *src, void *dst);

#endif
