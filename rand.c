/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "rand.h"

static uint32_t prng_seed;

void srand(uint32_t seed)
{
    prng_seed = seed;
}

uint32_t rand(void)
{
    register uint32_t seed = prng_seed;

    asm("or $t0,$zero,%[seed]\t\n"
        "sll $t1,$t0,13\t\n"
        "xor $t0,$t0,$t1\t\n"
        "srl $t1,$t0,7\t\n"
        "xor $t0,$t0,$t1\t\n"
        "sll $t1,$t0,17\t\n"
        "xor $t0,$t0,$t1\t\n"
        "or %[seed],$zero,$t0" : [seed] "+r"(seed));

    prng_seed = seed;
    return seed;
}
