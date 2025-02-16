/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

extern patch_t patch_ki1_whiteblood;
extern patch_t patch_kix_any_ide;
extern patch_t patch_ki2_uk_secbypass;
extern patch_t patch_ki2_2in1_hdd;
extern patch_t patch_kix_volume_fade;
extern patch_t patch_kix_remap;
extern patch_t patch_kix_reset;

patch_t *patches[] = {
#if defined(KI)
#if defined(KI_L15D) || defined(KI_L14) || defined(KI_L13)
    &patch_kix_any_ide,
#endif
#if defined(KI_L15DI) || defined(KI_L15D) || defined(KI_L14) || defined(KI_L13)
    &patch_kix_volume_fade,
#endif
#if defined(KI_L15D) || defined(KI_L15DI)
    &patch_ki1_whiteblood,
#endif
#if defined(KI_BOARD_20351) && (defined(KI_L15DI) || defined(KI_L15D) || defined(KI_L14) || defined(KI_L13))
    &patch_kix_remap,
#endif
    &patch_kix_reset,
#elif defined(KI2)
#if defined(KI2_L14) || defined(KI2_L14K) || defined(KI2_L13) || defined(KI2_L13K) || defined(KI2_L11) || defined(KI2_L10)
    &patch_kix_any_ide,
    &patch_kix_volume_fade,
#endif
#if defined(KI2_L14K) || defined(KI2_L13K)
    &patch_ki2_uk_secbypass,
#endif
#if defined(KI_BOARD_19489) && (defined(KI2_L10) || defined(KI2_L11))
    &patch_kix_remap,
#endif
    &patch_kix_reset,
#if defined(HDD_2IN1) || defined(ROM_2IN1)
    &patch_ki2_2in1_hdd,
#endif
#endif
    nullptr,
};
