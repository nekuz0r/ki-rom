/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _KI_H_
#define _KI_H_

#if defined(KI_BOARD_20351)
#define KI_BOARD_STR "A-20351"
#elif defined(KI_BOARD_19489)
#define KI_BOARD_STR "A-19489"
#else
#error "Please specify board type."
#endif

/**
 * KI_ROM_VERSION_STR is the long form the boot screen prints. KI_ROM_REVISION_STR
 * is just the revision, upper case, for the places that only have room for that
 * (the boot picker entries).
 */
#if defined(KI_L15DI)
#define KI_ROM_VERSION_STR "Killer Instinct l1.5di"
#define KI_ROM_REVISION_STR "L1.5DI"
#elif defined(KI_L15D)
#define KI_ROM_VERSION_STR "Killer Instinct l1.5d"
#define KI_ROM_REVISION_STR "L1.5D"
#elif defined(KI_L14)
#define KI_ROM_VERSION_STR "Killer Instinct l1.4"
#define KI_ROM_REVISION_STR "L1.4"
#elif defined(KI_L13)
#define KI_ROM_VERSION_STR "Killer Instinct l1.3"
#define KI_ROM_REVISION_STR "L1.3"
#elif defined(KI_P47)
#define KI_ROM_VERSION_STR "Killer Instinct p47"
#define KI_ROM_REVISION_STR "P47"
#elif defined(KI2_L14)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4"
#define KI_ROM_REVISION_STR "L1.4"
#elif defined(KI2_L14K)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4k"
#define KI_ROM_REVISION_STR "L1.4K"
#elif defined(KI2_L13)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.3"
#define KI_ROM_REVISION_STR "L1.3"
#elif defined(KI2_L13K)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.3k"
#define KI_ROM_REVISION_STR "L1.3K"
#elif defined(KI2_D14P)
#define KI_ROM_VERSION_STR "Killer Instinct 2 d1.4p"
#define KI_ROM_REVISION_STR "D1.4P"
#elif defined(KI2_L14P)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4p"
#define KI_ROM_REVISION_STR "L1.4P"
#elif defined(KI2_L11)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.1"
#define KI_ROM_REVISION_STR "L1.1"
#elif defined(KI2_L10)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.0"
#define KI_ROM_REVISION_STR "L1.0"
#else
#error "Please specify ROM version."
#endif

#endif
