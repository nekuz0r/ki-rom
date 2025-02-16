/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stdio.h>
#include "neku_font.h"

int main(int argc, const char **argv)
{
    printf("#pragma once\n");
    printf("#ifndef _FONT_H_\n");
    printf("#define _FONT_H_\n\n");
    printf("#define FONT_GLYPH_WIDTH (%d)\n", GLYPH_WIDTH);
    printf("#define FONT_GLYPH_HEIGHT (%d)\n\n", GLYPH_HEIGHT);
    printf("static const uint8_t font[] = {\n");
    for (int i = 0; i < 95; i++)
    {
        char *glyph = glyphArray[i];

        uint64_t output = 0;
        for (int j = 0; j < GLYPH_HEIGHT * GLYPH_WIDTH; j++)
        {
            output <<= 1;

            char c = glyph[j];
            if (c != ' ')
            {
                output |= 1;
            }
        }

        output <<= (64 - (GLYPH_HEIGHT * GLYPH_WIDTH));

        for (int b = 0; b < 8; b++)
        {
            printf("  0x%02x, ", ((uint8_t *)&output)[b]);
        }
        printf("\n");
    }
    printf("};\n\n");
    printf("#endif\n");
    return 0;
}
