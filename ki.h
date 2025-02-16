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

#if defined(KI_L15DI)
#define KI_ROM_VERSION_STR "Killer Instinct l1.5di"
#elif defined(KI_L15D)
#define KI_ROM_VERSION_STR "Killer Instinct l1.5d"
#elif defined(KI_L14)
#define KI_ROM_VERSION_STR "Killer Instinct l1.4"
#elif defined(KI_L13)
#define KI_ROM_VERSION_STR "Killer Instinct l1.3"
#elif defined(KI_P47)
#define KI_ROM_VERSION_STR "Killer Instinct p47"
#elif defined(KI2_L14)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4"
#elif defined(KI2_L14K)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4k"
#elif defined(KI2_L13)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.3"
#elif defined(KI2_L13K)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.3k"
#elif defined(KI2_D14P)
#define KI_ROM_VERSION_STR "Killer Instinct 2 d1.4p"
#elif defined(KI2_L14P)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.4p"
#elif defined(KI2_L11)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.1"
#elif defined(KI2_L10)
#define KI_ROM_VERSION_STR "Killer Instinct 2 l1.0"
#else
#error "Please specify ROM version."
#endif

#endif
