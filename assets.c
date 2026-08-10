/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stddef.h>
#include "assets.h"
#include "mem.h"
#include "lzss.h"

extern zrom_t zroms[ZROM_SEGMENTS];

uint8_t *zasset_ki1_characters[] = {
    zasset_ki1_cinder,
    zasset_ki1_combo,
    zasset_ki1_eyedol,
    zasset_ki1_fulgore,
    zasset_ki1_glacius,
    zasset_ki1_jago,
    zasset_ki1_orchid,
    zasset_ki1_riptor,
    zasset_ki1_sabrewulf,
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

const char *zasset_ki1_character_names[] = {
    "CINDER",
    "COMBO",
    "EYEDOL",
    "FULGORE",
    "GLACIUS",
    "JAGO",
    "ORCHID",
    "RIPTOR",
    "SABREWULF",
    "SPINAL",
    "THUNDER",
};

const char *zasset_ki2_character_names[] = {
    "COMBO",
    "FULGORE",
    "GARGOS",
    "GLACIUS",
    "JAGO",
    "KIM WU",
    "MAYA",
    "ORCHID",
    "SABREWULF",
    "SPINAL",
    "TUSK",
};

static_assert(sizeof(zasset_ki1_characters) / sizeof(zasset_ki1_characters[0]) == ZASSET_CHARACTERS);
static_assert(sizeof(zasset_ki2_characters) / sizeof(zasset_ki2_characters[0]) == ZASSET_CHARACTERS);
static_assert(sizeof(zasset_ki1_character_names) / sizeof(zasset_ki1_character_names[0]) == ZASSET_CHARACTERS);
static_assert(sizeof(zasset_ki2_character_names) / sizeof(zasset_ki2_character_names[0]) == ZASSET_CHARACTERS);

void *zasset_load(const void *ptr)
{
    void *out = malloc(lzss_decompressed_size(ptr));
    if (out != NULL)
    {
        lzss_decompress(ptr, out);
    }
    return out;
}

void zrom_load_segment(const uint8_t index)
{
    lzss_decompress(zroms[index].data, zroms[index].lma + 0x80000000UL);
}

void zrom_load(void)
{
    for (uint8_t i = 0; i < ZROM_SEGMENTS; i++)
    {
        zrom_load_segment(i);
    }
}
