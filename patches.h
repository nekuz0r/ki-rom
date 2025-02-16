/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _PATCH_H_
#define _PATCH_H_

#include <stdint.h>

typedef struct
{
  char *name;
  void (*apply)();
  bool status;
} patch_t;

extern patch_t *patches[];

#endif
