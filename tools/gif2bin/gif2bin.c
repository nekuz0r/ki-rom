#include <gif_lib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <libgen.h>
#include <string.h>

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_dir = argc >= 3 ? argv[2] : "";

    int error = 0;
    GifFileType *gif = DGifOpenFileName(input_file, &error);

    error = DGifSlurp(gif);

    printf("%d\n", gif->ImageCount);
    printf("%d\n", gif->SColorResolution);
    printf("%d\n", gif->SBackGroundColor);
    printf("%d\n", gif->AspectByte);

    char *input_filename = strdup(input_file);
    if (input_filename == NULL)
    {
        fprintf(stderr, "Error: strdup(%s)\n", input_file);
        DGifCloseFile(gif, &error);
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
        DGifCloseFile(gif, &error);
        return EXIT_FAILURE;
    }

    uint32_t width = gif->SavedImages[0].ImageDesc.Width;
    uint32_t height = gif->SavedImages[0].ImageDesc.Height;
    uint64_t imageCount = gif->ImageCount;

    fwrite(&width, sizeof(uint32_t), 1, ofp);
    fwrite(&height, sizeof(uint32_t), 1, ofp);
    fwrite(&imageCount, sizeof(uint64_t), 1, ofp);

    printf("BPP: %x\n", gif->SColorMap->BitsPerPixel);
    printf("CC: %x\n", gif->SColorMap->ColorCount);
    printf("%p\n", gif->SColorMap);
    printf("%x %x %x\n", gif->SColorMap->Colors[0].Red, gif->SColorMap->Colors[0].Green, gif->SColorMap->Colors[0].Blue);
    printf("%x %x %x\n", gif->SColorMap->Colors[1].Red, gif->SColorMap->Colors[1].Green, gif->SColorMap->Colors[1].Blue);
    printf("%x %x %x\n", gif->SColorMap->Colors[2].Red, gif->SColorMap->Colors[2].Green, gif->SColorMap->Colors[2].Blue);
    printf("%x %x %x\n", gif->SColorMap->Colors[3].Red, gif->SColorMap->Colors[3].Green, gif->SColorMap->Colors[3].Blue);
    for (uint8_t i = 0; i < gif->ImageCount; i++)
    {
        SavedImage *p = &gif->SavedImages[i];

        for (uint32_t j = 0; j < width * height; j++)
        {
            int idx = p->RasterBits[j];

            uint16_t c = 0x7C1F;
            if (idx != gif->SBackGroundColor)
            {
                GifColorType rgb = gif->SColorMap->Colors[idx];
                uint8_t r = rgb.Red >> 3;
                uint8_t g = rgb.Green >> 3;
                uint8_t b = rgb.Blue >> 3;
                c = ((b & 0x1F) << 10) | ((g & 0x1F) << 5) | (r & 0x1F);
            }

            fputc(c & 0xFF, ofp);
            fputc((c >> 8) & 0xFF, ofp);
        }
    }

    free(input_filename);
    fclose(ofp);
    error = DGifCloseFile(gif, &error);

    return EXIT_SUCCESS;
}
