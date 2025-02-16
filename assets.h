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

void *zasset_load(const void *ptr);
void zasset_clear(void);
void zrom_load(void);

#endif
