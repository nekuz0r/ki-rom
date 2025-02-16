/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

[[maybe_unused]] static void apply(void)
{
#if defined(KI_L15D)
    *(uint32_t *)0x880105c0 = 0x0a0041b5;
#elif defined(KI_L14)
    *(uint32_t *)0x880102f8 = 0x0a004103;
#elif defined(KI_L13)
    *(uint32_t *)0x880102e8 = 0x0a0040ff;
#elif defined(KI2_L14)
    *(uint32_t *)0x88000a8c = 0x0;
    *(uint32_t *)0x88000a9c = 0x0;
    *(uint32_t *)0x8802940c = 0x0;
    *(uint32_t *)0x8802941c = 0x0;
#elif defined(KI2_L14K)
    *(uint32_t *)0x88000a88 = 0x0;
    *(uint32_t *)0x88000a98 = 0x0;
    *(uint32_t *)0x880293dc = 0x0;
    *(uint32_t *)0x880293ec = 0x0;
#elif defined(KI2_L13)
    *(uint32_t *)0x88000a94 = 0x0;
    *(uint32_t *)0x88000aa4 = 0x0;
    *(uint32_t *)0x8802940c = 0x0;
    *(uint32_t *)0x8802941c = 0x0;
#elif defined(KI2_L13K)
    *(uint32_t *)0x88000a9c = 0x0;
    *(uint32_t *)0x88000aac = 0x0;
    *(uint32_t *)0x8802940c = 0x0;
    *(uint32_t *)0x8802941c = 0x0;
#elif defined(KI2_L11)
    *(uint32_t *)0x88000a80 = 0x0;
    *(uint32_t *)0x88000a90 = 0x0;
    *(uint32_t *)0x88029030 = 0x0;
    *(uint32_t *)0x88029040 = 0x0;
#elif defined(KI2_L10)
    *(uint32_t *)0x8800099c = 0x0;
    *(uint32_t *)0x880009ac = 0x0;
    *(uint32_t *)0x88029d40 = 0x0;
    *(uint32_t *)0x88029d50 = 0x0;
#endif
}

#if defined(KI_L15D) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L14K) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L11) || defined(KI2_L10)
patch_t patch_kix_any_ide = {
    .name = "{PATCH} AnyIDE",
    .apply = apply,
    .status = false,
};
#endif
