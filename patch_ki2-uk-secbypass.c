/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

[[maybe_unused]] static void apply(void)
{
#if defined(KI2_L14K)
    *(uint32_t *)0x880003d0 = 0x0;
    *(uint32_t *)0x88000490 = 0x0;
    *(uint32_t *)0x88000498 = 0x0;
    *(uint32_t *)0x88000b68 = 0x0;
    *(uint32_t *)0x88000b70 = 0x0;
#elif defined(KI2_L13K)
    *(uint32_t *)0x880003d8 = 0x0;
    *(uint32_t *)0x88000498 = 0x0;
    *(uint32_t *)0x880004a0 = 0x0;
    *(uint32_t *)0x88000b7c = 0x0;
    *(uint32_t *)0x88000b84 = 0x0;
#endif
}

#if defined(KI2_L14K) || defined(KI2_L13K)
patch_t patch_ki2_uk_secbypass = {
    .name = "{PATCH} A-20383 bypass",
    .apply = apply,
    .status = false,
};
#endif
