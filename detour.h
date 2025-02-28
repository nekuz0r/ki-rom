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

#define DETOUR_FN __attribute__((noinline, section(".detour.hook")))

#define DETOUR_INLINE_GATEWAY(name) \
    __attribute__((section(".detour.gateway"))) static uint32_t name##_gateway[4] = {0};

#define DETOUR_GATEWAY(name, dst)                \
    DETOUR_INLINE_GATEWAY(name)                  \
    DETOUR_FN static void name##_jump_gate(void) \
    {                                            \
        asm volatile(                            \
            "addiu $sp,$sp,-8\n"                 \
            "sd $ra,0($sp)\n"                    \
            "jal detour_save_registers\n"        \
            "nop\n"                              \
            "jal %[hooked]\n"                    \
            "nop\n"                              \
            "jal detour_load_registers\n"        \
            "nop\n"                              \
            "ld $ra,0($sp)\n"                    \
            "addiu $sp,$sp,8\n"                  \
            : : [hooked] "i"(&dst));             \
    }

#define DETOUR_INT_GATEWAY(name, dst)                                \
    DETOUR_INLINE_GATEWAY(name)                                      \
    DETOUR_FN static void name##_jump_gate(void)                     \
    {                                                                \
        asm volatile(                                                \
            ".set noreorder\n"                                       \
            "addiu $sp,$sp,-8\n"                                     \
            "sd $ra,0($sp)\n"                                        \
            "jal detour_save_registers\n"                            \
            "nop\n"                                                  \
            "jal %[hooked]\n"                                        \
            "nop\n"                                                  \
            "jal detour_load_registers\n"                            \
            "nop\n"                                                  \
            "ld $ra,0($sp)\n"                                        \
            "addiu $sp,$sp,8\n"                                      \
            "j %[gateway]\n"                                         \
            "nop\n"                                                  \
            : : [hooked] "i"(&dst), [gateway] "i"(&name##_gateway)); \
    }

#define DETOUR_INLINE(name, src, dst) detour(name##_gateway, (void *)(src), (void *)(dst))
#define DETOUR(name, src) detour(name##_gateway, (void *)(src), name##_jump_gate)

#define DETOUR_RETURN(name, reg)                                       \
    asm volatile("j %[gateway]\n" : : [gateway] "i"(&name##_gateway)); \
    asm volatile("nop\n")

void *detour(uint32_t *gateway, uint32_t *src, void *dst);

#endif
