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

uint8_t *zasset_ki1_characters[] = {
    zasset_ki1_cinder,
    zasset_ki1_combo,
    zasset_ki1_eyedol,
    zasset_ki1_fulgore,
    zasset_ki1_glacius,
    zasset_ki1_jago,
    zasset_ki1_orchid,
    zasset_ki1_riptor,
    zasset_ki1_sabrewolf,
    zasset_ki1_spinal,
    zasset_ki1_thunder,
};

uint8_t *zasset_ki2_characters[] = {
    zasset_ki2_combo,
    zasset_ki2_fulgore,
    zasset_ki2_gargos,
    zasset_ki2_glacius,
    zasset_ki2_jago,
    zasset_ki2_kim_wu,
    zasset_ki2_maya,
    zasset_ki2_orchid,
    zasset_ki2_sabrewulf,
    zasset_ki2_spinal,
    zasset_ki2_tusk,
};

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
