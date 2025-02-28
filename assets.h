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
extern uint8_t zasset_ki1_sabrewolf[];
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

extern uint8_t *zasset_ki1_characters[];
extern uint8_t *zasset_ki2_characters[];

void *zasset_load(const void *ptr);
void zrom_load(void);

#endif
