/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

uint32_t checksum32(const uint8_t *ptr, uint32_t length)
{
    const uint8_t *cur_ptr = ptr;
    const uint8_t *end_ptr = ptr + length;

    uint64_t value = 1;
    do
    {
        value = ((value << 0x3f) >> 0x1f | (value << 0x1f) >> 0x20) ^ (value << 0x2c) >> 0x20;
        value = value >> 0x14 & 0xfff ^ value ^ *cur_ptr;
        cur_ptr++;
    } while (cur_ptr != end_ptr);
    return value & 0xffffffff;
}

int main(int argc, const char **argv)
{
    if (argc < 4)
    {
        printf("Usage: %s diskfile offset block-size\n", argv[0]);
        printf("\tdiskfile\t: path to the binary disk file\n");
        printf("\toffset\t: offset to start checksuming at (sector count in hexadecimal)\n");
        printf("\tblock-size\t: size of a checksumed block (sector count in hexadecimal)\n");
        exit(1);
    }

    const char *diskfile = argv[1];
    uint32_t offset = strtoul(argv[2], NULL, 16);
    uint32_t length = strtoul(argv[3], NULL, 16);

    FILE *fp = fopen(diskfile, "rb");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", diskfile);
        exit(1);
    }
    fseek(fp, offset * 512, SEEK_SET);

    uint8_t *buffer = (uint8_t *)malloc(length * 512);

    for (uint8_t sector = 0; sector < 0x80; sector++)
    {
        fread(buffer, 512, length, fp);
        uint32_t checksum = checksum32(buffer, length * 512);
        printf("0x%08x\n", checksum);
    }

    free(buffer);
    fclose(fp);
    return 0;
}
