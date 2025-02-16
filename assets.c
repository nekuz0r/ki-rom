/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stddef.h>
#include "assets.h"
#include "mem.h"
#include "lzss.h"

extern uint8_t gAssetBuffer[0x2800];
extern zrom_t zroms[3];
static void *buffer_ptr = gAssetBuffer;

void *zasset_load(const void *ptr)
{
    void *out = buffer_ptr;
    buffer_ptr = lzss_decompress(ptr, out);
    return out;
}

void zasset_clear(void)
{
    buffer_ptr = gAssetBuffer;
    memset(gAssetBuffer, 0, sizeof(gAssetBuffer));
}

void zrom_load(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        lzss_decompress(zroms[i].data, zroms[i].lma + 0x80000000UL);
    }
}
