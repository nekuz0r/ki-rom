/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stddef.h>
#include "assets.h"
#include "mem.h"
#include "lzss.h"

extern zrom_t zroms[3];

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
    void *out = malloc(lzss_decompressed_size(ptr));
    if (out != NULL)
    {
        lzss_decompress(ptr, out);
    }
    return out;
}

void zrom_load(void)
{
    lzss_decompress(zroms[0].data, zroms[0].lma + 0x80000000UL);
    lzss_decompress(zroms[1].data, zroms[1].lma + 0x80000000UL);
    lzss_decompress(zroms[2].data, zroms[2].lma + 0x80000000UL);
}
