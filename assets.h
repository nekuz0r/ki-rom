/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _ASSETS_H_
#define _ASSETS_H_

#include <stdint.h>

typedef struct
{
    void *lma;
    void *data;
} zrom_t;

extern uint8_t zasset_logo[];
extern uint8_t zasset_ki1_cinder[];
extern uint8_t zasset_ki1_combo[];
extern uint8_t zasset_ki1_eyedol[];
extern uint8_t zasset_ki1_fulgore[];
extern uint8_t zasset_ki1_glacius[];
extern uint8_t zasset_ki1_jago[];
extern uint8_t zasset_ki1_orchid[];
extern uint8_t zasset_ki1_riptor[];
extern uint8_t zasset_ki1_sabrewulf[];
extern uint8_t zasset_ki1_spinal[];
extern uint8_t zasset_ki1_thunder[];
extern uint8_t zasset_ki2_combo[];
extern uint8_t zasset_ki2_fulgore[];
extern uint8_t zasset_ki2_gargos[];
extern uint8_t zasset_ki2_glacius[];
extern uint8_t zasset_ki2_jago[];
extern uint8_t zasset_ki2_kim_wu[];
extern uint8_t zasset_ki2_maya[];
extern uint8_t zasset_ki2_orchid[];
extern uint8_t zasset_ki2_sabrewulf[];
extern uint8_t zasset_ki2_spinal[];
extern uint8_t zasset_ki2_tusk[];
extern uint8_t zasset_rare_logo[];
extern uint8_t zasset_nintendo_logo[];

#define ZASSET_CHARACTERS (11)

extern uint8_t *zasset_ki1_characters[];
extern uint8_t *zasset_ki2_characters[];

// Parallel to the arrays above: index i names the fighter that index i draws.
extern const char *zasset_ki1_character_names[];
extern const char *zasset_ki2_character_names[];

#define ZROM_SEGMENTS (3)

void *zasset_load(const void *ptr);

/**
 * One segment at a time. Decompressing all three in a single call blocks for
 * seconds with nothing on screen changing; a view that wants to show progress
 * calls this once per frame instead.
 */
void zrom_load_segment(const uint8_t index);
void zrom_load(void);

#endif
