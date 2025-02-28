/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"
#include "io.h"
#include "view.h"
#include "mem.h"
#include "detour.h"
#include "sound.h"
#include "cache.h"

extern void _reset(view_t *view) __attribute__((far));

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

static void (*org_interrupt_handler)(void) = (void *)0x88023b04;

DETOUR_FN static void interrupt_handler_hooked(void)
{
    if ((gIO.player1 & 0x444) != 0)
    {
        return;
    }

    gIO.soundReset = 0;

    uint32_t addr = 0x80000000;
    do
    {
        CACHE_OP(INDEX_WRITEBACK_INVALIDATE_D, addr, 0x0000);
        CACHE_OP(INDEX_WRITEBACK_INVALIDATE_D, addr, 0x2000);
        addr += 32;
    } while (addr != 0x80002000);
    SYNC();

    _reset(
#if defined(ROM_2IN1)
        &view_bootselect
#else
        &view_main
#endif
    );
}
DETOUR_INT_GATEWAY(interrupt_handler, interrupt_handler_hooked);

[[maybe_unused]] static void apply(void)
{
#if defined(KI_L15D) || defined(KI_L15DI) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L14K) || defined(KI2_L11) || defined(KI2_L10)
    DETOUR(interrupt_handler, org_interrupt_handler);
#endif
}

#if defined(KI_L15D) || defined(KI_L15DI) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L14K) || defined(KI2_L11) || defined(KI2_L10)
patch_t patch_kix_reset = {
    .name = "{PATCH} Soft reset (UP1 + S1 + FP1)",
    .apply = apply,
    .status = false,
};
#endif
