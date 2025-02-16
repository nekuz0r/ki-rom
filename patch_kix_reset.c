/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"
#include "io.h"
#include "view.h"
#include "mem.h"
#include "detour.h"

#if defined(KI_L15DI)
static constexpr uintptr_t hook_target_addr = 0x880266dc;
#elif defined(KI_L15D)
static constexpr uintptr_t hook_target_addr = 0x880267ec;
#elif defined(KI_L14)
static constexpr uintptr_t hook_target_addr = 0x880264ac;
#elif defined(KI_L13)
static constexpr uintptr_t hook_target_addr = 0x8802649c;
#elif defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K)
static constexpr uintptr_t hook_target_addr = 0x88023b9c;
#elif defined(KI2_L14K)
static constexpr uintptr_t hook_target_addr = 0x88023b6c;
#elif defined(KI2_L11)
static constexpr uintptr_t hook_target_addr = 0x88023a3c;
#elif defined(KI2_L10)
static constexpr uintptr_t hook_target_addr = 0x880238d0;
#endif

DETOUR_GATEWAY(reset);

[[maybe_unused]] DETOUR_FN void reset_hook(void)
{
    asm volatile(
        ".set noat\n"
        ".set noreorder\n"
        "la $a0,%[buttons]\n"
        "lw $a0,0x0($a0)\n"
        "andi $a0,$a0,0x444\n"
        "bnez $a0,exit\n"
        "nop\n"
        "la $a0,%[sound_reset]\n"
        "li $a1,0x0\n"
        "sb $a1,0x0($a0)\n"
        "la $k0,%[view]\n"
        "lui $k1,0x2000\n"
        "la $ra,_reset\n"
        "or $ra,$ra,$k1\n" // Make sure we jump back to uncached memory
        "jr $ra\n"
        "or $k1,$k1,$0\n"
        "exit:\n" : : [buttons] "i"(&gIO.player1), [sound_reset] "i"(&gIO.soundReset), [view] "i"(&view_main));

    DETOUR_RETURN(reset, a0);
}

[[maybe_unused]] static void apply(void)
{
#if defined(KI_L15D) || defined(KI_L15DI) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L14K) || defined(KI2_L11) || defined(KI2_L10)
    DETOUR(reset, hook_target_addr, &reset_hook);
#endif
}

#if defined(KI_L15D) || defined(KI_L15DI) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L14K) || defined(KI2_L11) || defined(KI2_L10)
patch_t patch_kix_reset = {
    .name = "{PATCH} Soft reset (UP1 + S1 + FP1)",
    .apply = apply,
    .status = false,
};
#endif
