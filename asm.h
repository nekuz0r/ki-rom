/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _ASM_H_
#define _ASM_H_

#define NOP() (0x0)
#define LUI(rs, immediate) ((0xF << 26) | ((rs) << 16) | ((immediate) & 0xFFFF))
#define ORI(rt, rs, immediate) \
  ((0xD << 26) | ((rs) << 21) | ((rt) << 16) | ((immediate) & 0xFFFF))
#define JR(rs) (((rs) << 21) | 0x8)
#define J(rel) ((0x2 << 26) | (rel))
#define REL_ADDR(dst) (((uintptr_t)(dst) & 0x0FFFFFFF) >> 2)

#endif
