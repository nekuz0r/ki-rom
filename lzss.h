/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _LZSS_H_
#define _LZSS_H_

uint32_t lzss_decompressed_size(const void *src);
void *lzss_decompress(register const void *src, register void *dst);

#endif
