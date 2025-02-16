/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

[[maybe_unused]] static void apply(void)
{
#if defined(KI_L15D) || defined(KI_L15DI)
    *(uint8_t *)0x880003ca = 0x20;
#endif
}

#if defined(KI_L15D) || defined(KI_L15DI)
patch_t patch_ki1_whiteblood = {
    .name = "{PATCH} No whiteblood",
    .apply = apply,
    .status = false,
};
#endif
