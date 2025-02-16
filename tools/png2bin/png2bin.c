/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <png.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <libgen.h>
#include <string.h>

#define PNG_HEADER_LEN 8

int main(int argc, const char **argv)
{
    if (argc < 1)
    {
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_dir = argc >= 2 ? argv[2] : "";

    FILE *fp = fopen(input_file, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error: fopen(%s, 'rb')\n", input_file);
        return EXIT_FAILURE;
    }

    unsigned char header[PNG_HEADER_LEN];
    if (fread(header, 1, PNG_HEADER_LEN, fp) < PNG_HEADER_LEN)
    {
        fprintf(stderr, "Error: fread(len = %d)\n", PNG_HEADER_LEN);
        fclose(fp);
        return EXIT_FAILURE;
    }

    int is_png = !png_sig_cmp(header, 0, PNG_HEADER_LEN);
    if (!is_png)
    {
        fprintf(stderr, "Error: png_sig_cmp\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    png_structp pngptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop pnginfo = png_create_info_struct(pngptr);
    if (!pnginfo)
    {
        fprintf(stderr, "Error: png_create_info_struct\n");
        fclose(fp);
        png_destroy_read_struct(&pngptr, NULL, NULL);
        return EXIT_FAILURE;
    }
    png_set_palette_to_rgb(pngptr);

    png_init_io(pngptr, fp);
    png_set_sig_bytes(pngptr, PNG_HEADER_LEN);
    png_read_png(pngptr, pnginfo, PNG_TRANSFORM_IDENTITY, NULL);
    png_bytepp rows = png_get_rows(pngptr, pnginfo);

    uint32_t width, height;
    int32_t bit_depth, color_type;
    if (png_get_IHDR(pngptr, pnginfo, &width, &height, &bit_depth,
                     &color_type, NULL, NULL, NULL) == 0)
    {
        fprintf(stderr, "Error: png_get_IHDR\n");
        fclose(fp);
        png_destroy_read_struct(&pngptr, NULL, NULL);
        return EXIT_FAILURE;
    }

    char *input_filename = strdup(input_file);
    if (input_filename == NULL)
    {
        fprintf(stderr, "Error: strdup(%s)\n", input_file);
        fclose(fp);
        png_destroy_read_struct(&pngptr, NULL, NULL);
        return EXIT_FAILURE;
    }

    char *pos = strrchr(input_filename, '.');
    if (pos != NULL)
    {
        *pos = 0;
    }

    char output_filename[256] = {0};
    snprintf(output_filename, 256, "%s%s.bin", output_dir, input_filename);
    FILE *ofp = fopen(output_filename, "wb");
    if (ofp == NULL)
    {
        fprintf(stderr, "Error: fopen(%s, 'wb')\n", output_filename);
        free(input_filename);
        fclose(fp);
        png_destroy_read_struct(&pngptr, &pnginfo, NULL);
        pngptr = NULL;
        pnginfo = NULL;
        return EXIT_FAILURE;
    }

    fwrite(&width, sizeof(uint32_t), 1, ofp);
    fwrite(&height, sizeof(uint32_t), 1, ofp);

    for (uint32_t i = 0; i < height; i++)
    {
        for (uint32_t j = 0; j < width * 3; j += 3)
        {
            uint8_t r = (rows[i][j] * 31) >> bit_depth;
            uint8_t g = (rows[i][j + 1] * 31) >> bit_depth;
            uint8_t b = (rows[i][j + 2] * 31) >> bit_depth;

            uint16_t c = ((b & 0x1F) << 10) | ((g & 0x1F) << 5) | (r & 0x1F);

            fputc(c & 0xFF, ofp);
            fputc((c >> 8) & 0xFF, ofp);
        }
    }
    free(input_filename);
    fclose(fp);
    fclose(ofp);

    if (pngptr && pnginfo)
    {
        png_destroy_read_struct(&pngptr, &pnginfo, NULL);
        pngptr = NULL;
        pnginfo = NULL;
    }

    return EXIT_SUCCESS;
}
