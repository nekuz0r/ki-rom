/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

[[maybe_unused]] static void apply(void)
{
#if defined(KI_L15DI)
    *(uint16_t *)0x8802bf24 = 0x0000;
#elif defined(KI_L15D)
    *(uint16_t *)0x8802c034 = 0x0000;
#elif defined(KI_L14)
    *(uint16_t *)0x8802b890 = 0x0000;
#elif defined(KI_L13)
    *(uint16_t *)0x8802b878 = 0x0000;
#elif defined(KI2_L14) || defined(KI2_L13) || defined(KI2_L13K)
    *(uint16_t *)0x8802a708 = 0x0000;
#elif defined(KI2_L14K)
    *(uint16_t *)0x8802a6d8 = 0x0000;
#elif defined(KI2_L10)
    *(uint16_t *)0x88029fb8 = 0x0000;
#elif defined(KI2_L11)
    *(uint16_t *)0x8802a32c = 0x0000;
#endif
}

#if defined(KI_L15DI) || defined(KI_L15D) || defined(KI_L14) || defined(KI_L13) || defined(KI2_L14) || defined(KI2_L14K) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L10) || defined(KI2_L11)
patch_t patch_kix_volume_fade = {
    .name = "{PATCH} No attract sound fading",
    .apply = apply,
    .status = false,
};
#endif
