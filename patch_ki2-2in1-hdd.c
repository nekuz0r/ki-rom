/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"
#include "mem.h"
#include "detour.h"

/*
#if defined(KI_L15D)
static constexpr uintptr_t ide_init_addr = 0x8802d8d0;
static constexpr uintptr_t ide_seek_addr = 0x8802db58;
#elif defined(KI_L15DI)
static constexpr uintptr_t ide_init_addr = 0x8802d7c0;
static constexpr uintptr_t ide_seek_addr = 0x8802da48;
#elif defined(KI_L14)
static constexpr uintptr_t ide_init_addr = 0x8802D130;
static constexpr uintptr_t ide_seek_addr = 0x8802D3B8;
#elif defined(KI_L13)
static constexpr uintptr_t ide_init_addr = 0x8802D110;
static constexpr uintptr_t ide_seek_addr = 0x8802D398;
#endif
*/

#if defined(KI2) && (defined(HDD_2IN1) || defined(ROM_2IN1))
// game
#if defined(KI2_L14) || defined(KI2_L14P) || defined(KI2_L13) || defined(KI2_L13K)
// static constexpr uintptr_t ide_init_addr = 0x8801BA60;
static constexpr uintptr_t ide_seek_addr = 0x8801BD1C;
#elif defined(KI2_L14K)
// static constexpr uintptr_t ide_init_addr = 0x8801BA40;
static constexpr uintptr_t ide_seek_addr = 0x8801BCFC;
#elif defined(KI2_L11)
// static constexpr uintptr_t ide_init_addr = 0x8801B910;
static constexpr uintptr_t ide_seek_addr = 0x8801BBCC;
#elif defined(KI2_L10)
// static constexpr uintptr_t ide_init_addr = 0x8801B7B0;
static constexpr uintptr_t ide_seek_addr = 0x8801BA6C;
#endif

// check disk test
#if defined(KI2_L14) || defined(KI2_L14P) || defined(KI2_L13) || defined(KI2_L13K)
static constexpr uintptr_t test_disk_end_sector_low_addr = 0x88020ECC;
static constexpr uintptr_t test_disk_end_sector_high_addr = 0x88020ED0;
static constexpr uintptr_t test_disk_block_size_sector_addr = 0x88020F08;
static constexpr uintptr_t test_disk_block_size_sector_addr_2 = 0x88020F28;
static constexpr uintptr_t test_disk_block_size_bytes_addr = 0x88020F20;
#elif defined(KI2_L14K)
static constexpr uintptr_t test_disk_end_sector_low_addr = 0x88020EA8;
static constexpr uintptr_t test_disk_end_sector_high_addr = 0x88020EAC;
static constexpr uintptr_t test_disk_block_size_sector_addr = 0x88020EE4;
static constexpr uintptr_t test_disk_block_size_sector_addr_2 = 0x88020F04;
static constexpr uintptr_t test_disk_block_size_bytes_addr = 0x88020EFC;
#elif defined(KI2_L11)
static constexpr uintptr_t test_disk_end_sector_low_addr = 0x88020D68;
static constexpr uintptr_t test_disk_end_sector_high_addr = 0x88020D6C;
static constexpr uintptr_t test_disk_block_size_sector_addr = 0x88020DA4;
static constexpr uintptr_t test_disk_block_size_sector_addr_2 = 0x88020DC4;
static constexpr uintptr_t test_disk_block_size_bytes_addr = 0x88020DBC;
#elif defined(KI2_L10)
static constexpr uintptr_t test_disk_end_sector_low_addr = 0x88020C04;
static constexpr uintptr_t test_disk_end_sector_high_addr = 0x88020C08;
static constexpr uintptr_t test_disk_block_size_sector_addr = 0x88020C40;
static constexpr uintptr_t test_disk_block_size_sector_addr_2 = 0x88020C60;
static constexpr uintptr_t test_disk_block_size_bytes_addr = 0x88020C58;
#endif

/**
 * Computed by disk-checksum tool
 * disk-checksum ki2.hd.bin 0x14 0xC80
 */
static uint32_t disk_checksums[] = {
    0x8899f4a2, 0x6ea1dc19, 0x1f1b2b40, 0x8d730faa, 0x15702fd4, 0x2e8a8b44, 0x69efafc1, 0x5a9a8df2,
    0x35ebc95e, 0x60caed77, 0x9d4a2c6a, 0xfe9dd4a7, 0x76b85b33, 0x219f9479, 0x735af18f, 0x681fac7e,
    0x6b5e6fe0, 0xb6ca5f48, 0xde640cd6, 0x453b07a7, 0x6afac3fa, 0x177cfdf0, 0x54b1804c, 0x0c83103b,
    0x3b84a7ed, 0x0d5269a7, 0x2c83ee44, 0x3c7aeae1, 0x12bc1740, 0x8afc06d1, 0x57a2c114, 0xa66b3051,
    0x86d36531, 0x26d11085, 0xa81e970a, 0x1a06055a, 0x60643355, 0xee6d0bb5, 0xb912b992, 0x8e1515fb,
    0x05f39ab7, 0xe0d7a899, 0xa76a76d2, 0x34b95e6b, 0xf0f79b90, 0xe9231406, 0x1bf64b71, 0x9cf0da0e,
    0xc0474573, 0x7850a2f2, 0xa1870860, 0x848a43a8, 0xe4067b65, 0xc74eef16, 0xd57caa22, 0x2726bbe5,
    0x37933885, 0xff8d70c2, 0xd7f8779c, 0xb174caa6, 0xdcc9aad1, 0xb5d48e63, 0xb3790de8, 0xfc12b79c,
    0x67b733d3, 0xf07cac3c, 0xe6917ade, 0x0f9cd2ed, 0x3b52ec73, 0x5bc43e62, 0x34ccec80, 0xcd8f314c,
    0xf5a44e84, 0x606bcd9b, 0x1847c3c1, 0xee55f9c9, 0x1557886b, 0x9eaf983e, 0x021699ab, 0x955093d1,
    0xb74e7329, 0x85056300, 0x9c23f554, 0xd0b894ba, 0xe13d8027, 0xfc1753c0, 0xe81e97da, 0xd6933f7a,
    0x4b8c614a, 0x5a0b908d, 0x85ea6385, 0x36ad651c, 0x25f2c2a9, 0x1619fe28, 0xed721b93, 0x949551f0,
    0xd3f8d71b, 0x083f8038, 0x91a87f3a, 0xbe67dfd3, 0xb8e3a0bf, 0x08e269e9, 0xf3382bcf, 0x121f9aa5,
    0x82457a76, 0xda7e055d, 0x1e67368a, 0xb6c711e9, 0x959f0a2a, 0xb1e15fa0, 0x08de9183, 0xc6ecab4d,
    0xd11c1ab8, 0x4c2b80f9, 0xc74775d9, 0x7aa78b52, 0xaa944375, 0x4ccb77ab, 0x0fea45c1, 0xf10eaaf2,
    0xf10eaaf2, 0xfb03d90f, 0x0681ded7, 0xcc218659, 0x38ebcc0a, 0xdadab479, 0x523e279e, 0xe7ce0120};

DETOUR_GATEWAY(entry);
DETOUR_GATEWAY(exit);

[[maybe_unused]] DETOUR_FN static void ide_seek_entry(void)
{
    asm volatile("or $k0,$0,$a1\n"    // Save a1 register (restored on exit)
                 "la $v0,%[offset]\n" // Offset LBA address
                 "addu $a1,$a1,$v0\n"
                 : : [offset] "i"(0x0003E809));
    DETOUR_RETURN(entry, v0);
}

[[maybe_unused]] DETOUR_FN static void ide_seek_exit(void)
{
    asm volatile("or $a1,$0,$k0\n"); // Restore a1 register
    DETOUR_RETURN(exit, k0);
}

[[maybe_unused]] static void apply(void)
{
    //*(uint8_t *)(ide_init_addr + 0x14) = 0x40;
    //*(uint8_t *)(ide_seek_addr + 0x14) = 0x40;

    DETOUR(entry, ide_seek_addr, &ide_seek_entry);
    DETOUR(exit, ide_seek_addr + 0x60, &ide_seek_exit);

    /**
     * KI2 HDD checksumed data starts at sector 0x14 (0x14 * 0x200 = 0x2800 bytes)
     * The disk content ends at sector 0x63A33 (0xC746600 bytes)
     * But we need 0x80 block checksums with two constraints:
     * - NumberOfBlock * 0x80 must be a multiple of 0x10000 (0x80 * 0x200 = 0x10000)
     *      - The number of checksums fixes the minimum number of blocks to 0x80 (128)
     *      - The sector size fixes the minimum size of a block to 0x200 (512)
     * - BlockSize * 0x200 must be a multiple of 0x10000
     * The next value validating these two rules is 0xC800000 (bytes to read)
     * Length = 0xC800000
     * SectorsPerBlock = 0xC800000 / 0x200 / 0x80 = 0xC80
     * BlockBytes = (0xC800000 / 0x80) = 0x19000 (0x19000 >> 0x10 = 0x19)
     * LastSector = 0x14 + (0xC800000 / 0x200) = 0x14 + 0x64000 = 0x64014
     * The HDD can thus be truncated at (0x64014 * 0x200 = 0xC802800 bytes)
     */
    *(uint8_t *)test_disk_end_sector_low_addr = 0x06;
    *(uint16_t *)test_disk_end_sector_high_addr = 0x4014;
    *(uint16_t *)test_disk_block_size_sector_addr = 0x0C80;
    *(uint16_t *)test_disk_block_size_sector_addr_2 = 0x0C80;
    *(uint8_t *)test_disk_block_size_bytes_addr = 0x19;

    uint32_t *p = (void *)0x8803B2C8;
    for (uint8_t i = 0; i < sizeof(disk_checksums) / sizeof(uint32_t); i++)
    {
        p[i] = disk_checksums[i];
    }
}

patch_t patch_ki2_2in1_hdd = {
    .name = "{PATCH} HDD 2in1",
    .apply = apply,
    .status = false,
};
#endif
