/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "math.h"

__attribute__((const)) int abs(int x)
{
    return x < 0 ? -x : x;
}

__attribute__((const)) long int labs(long int x)
{
    return x < 0 ? -x : x;
}

__attribute__((const)) long long int llabs(long long int x)
{
    return x < 0 ? -x : x;
}

__attribute__((const)) float ceil(float value)
{
    int integer_value = value;
    if ((float)integer_value != value)
        return value + 1;
    return value;
}
