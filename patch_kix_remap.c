/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "patches.h"

static uint8_t mapping[] = {
#if defined(KI_BOARD_20351)
    0x98, 0x90, 0x80, 0xa0, 0x88, 0x00, 0xb8, 0x00
#elif defined(KI_BOARD_19489)
    0x90, 0xa0, 0x88, 0x80, 0x98, 0x00, 0x00, 0xb0
#endif
};

// 0x88000000 + offset
static uint16_t offsets_0[] = {
#if defined(KI_BOARD_19489)
#if defined(KI2_L10)
    0x01bc, 0x037c, 0x03ec, 0x0440, 0x053c, 0x0550, 0x0568, 0x0b20,
    0x21d0, 0x5b18, 0x5b40, 0x5b4c, 0x5b68, 0x5b88, 0x5bbc, 0x5bc8,
    0x5be4, 0x5c14, 0x5c30, 0x5c50, 0x5ca8, 0x5cd0, 0x5cdc, 0x5cf8,
    0x5d18, 0x5d50, 0x5d5c, 0x5d78, 0xf4b0, 0xf4f0
#elif (defined(KI2_L11))
    0x01bc, 0x03a4, 0x0414, 0x0468, 0x0564, 0x0578, 0x0590, 0x0c04,
    0x22b4, 0x5c00, 0x5c28, 0x5c34, 0x5c50, 0x5c70, 0x5ca4, 0x5cb0,
    0x5ccc, 0x5cfc, 0x5d18, 0x5d38, 0x5d90, 0x5db8, 0x5dc4, 0x5de0,
    0x5e00, 0x5e38, 0x5e44, 0x5e60, 0xf5fc, 0xf63c
#endif
#elif defined(KI_BOARD_20351)
#if defined(KI_L13)
    0x029c, 0x031c, 0x039c, 0x03b4, 0x07f8, 0x3db0, 0x3dd8, 0x3de4, 0x3e00, 0x3e20, 0x3e54, 0x3e60, 0x3e7c, 0x3eac, 0x3ec8, 0x3ee8, 0x3f40, 0x3f68, 0x3f74, 0x3f90, 0x3fb0, 0x3fe8, 0x3ff4, 0x4010, 0x800c, 0x805c, 0x8060
#elif defined(KI_L14)
    0x029c, 0x031c, 0x03a0, 0x03b8, 0x07fc, 0x3db4, 0x3ddc, 0x3de8, 0x3e04, 0x3e24, 0x3e58, 0x3e64, 0x3e80, 0x3eb0, 0x3ecc, 0x3eec, 0x3f44, 0x3f6c, 0x3f78, 0x3f94, 0x3fb4, 0x3fec, 0x3ff8, 0x4014, 0x8010, 0x8060, 0x8064
#elif defined(KI_L15D)
    0x029c, 0x031c, 0x039c, 0x03b4, 0x03d0, 0x0814, 0x3e6c, 0x3e94, 0x3ea0, 0x3ebc, 0x3edc, 0x3f10, 0x3f1c, 0x3f38, 0x3f68, 0x3f84, 0x3fa4, 0x3ffc, 0x4024, 0x4030, 0x404c, 0x406c, 0x40a4, 0x40b0, 0x40cc, 0x8240, 0x8290, 0x8294
#elif defined(KI_L15DI)
    0x029c, 0x031c, 0x039c, 0x03b4, 0x03d0, 0x0814, 0x3e6c, 0x3e94, 0x3ea0, 0x3ebc, 0x3edc, 0x3f10, 0x3f1c, 0x3f38, 0x3f68, 0x3f84, 0x3fa4, 0x3ffc, 0x4024, 0x4030, 0x404c, 0x406c, 0x40a4, 0x40b0, 0x40cc, 0x8240, 0x8290, 0x8294
#endif
#endif
};

// 0x88010000 + offset
static uint16_t offsets_1[] = {
#if defined(KI_BOARD_19489)
#if defined(KI2_L10)
    0xa0a0, 0xba24, 0xbb08, 0xbb34, 0xbb5c, 0xbb84, 0xc750
#elif (defined(KI2_L11))
    0xa200, 0xbb84, 0xbc68, 0xbc94, 0xbcbc, 0xbce4, 0xc8b0
#endif
#elif defined(KI_BOARD_20351)
#if defined(KI_L13)
    0x23a0, 0x7724, 0xd324, 0xd498, 0xd620, 0xd668, 0xdc90,
    0xdc94, 0xdd4c, 0xdd58, 0xdd5c, 0xe85c, 0xe878, 0xea6c,
    0xec04, 0xecd0, 0xed10, 0xeee0, 0xef4c, 0xf14c, 0xf200,
    0xf73c, 0xf8c0, 0xf8c4
#elif defined(KI_L14)
    0x23b0, 0x7734, 0xd334, 0xd4a8, 0xd630, 0xd678, 0xdca0,
    0xdca4, 0xdd5c, 0xdd68, 0xdd6c, 0xe86c, 0xe888, 0xea7c,
    0xec14, 0xece0, 0xed20, 0xeef0, 0xef5c, 0xf15c, 0xf210,
    0xf74c, 0xf8d0, 0xf8d4
#elif defined(KI_L15D)
    0x2680, 0x7a14, 0xd614, 0xd788, 0xd910, 0xd958, 0xdf80,
    0xdf84, 0xe03c, 0xe048, 0xe04c, 0xeb4c, 0xeb68, 0xed5c,
    0xeef4, 0xefc0, 0xf000, 0xf1d0, 0xf23c, 0xf43c, 0xf4f0,
    0xfa2c, 0xfbb0, 0xfbb4
#elif defined(KI_L15DI)
    0x2570, 0x7904, 0xd504, 0xd678, 0xd800, 0xd848, 0xde70,
    0xde74, 0xdf2c, 0xdf38, 0xdf3c, 0xea3c, 0xea58, 0xec4c,
    0xede4, 0xeeb0, 0xeef0, 0xf0c0, 0xf12c, 0xf32c, 0xf3e0,
    0xf91c, 0xfaa0, 0xfaa4
#endif
#endif
};

// 0x88020000 + offset
static uint16_t offsets_2[] = {
#if defined(KI_BOARD_19489)
#if defined(KI2_L10)
    0x0014, 0x0188, 0x0310, 0x0358, 0x09b0, 0x09b4, 0x0a6c, 0x0a78,
    0x0a7c, 0x15ec, 0x1608, 0x17fc, 0x1994, 0x1a60, 0x1aa0, 0x1c70,
    0x1cdc, 0x1ee0, 0x1f94, 0x24d0, 0x2654, 0x2658, 0x2fcc, 0x2fd0,
    0x30a0, 0x30a4, 0x326c, 0x3720, 0x3b94, 0x3c2c, 0x3c4c, 0x3c6c,
    0x3d00, 0x3d24, 0x3d7c, 0x3da0, 0x5af4, 0x5afc, 0x5fb0, 0x64a4,
    0x65f4, 0x661c, 0x6684, 0x6698, 0x914c, 0x9150, 0x9204, 0x97d8,
    0x9820, 0x983c, 0x9978, 0x998c, 0x9c00, 0x9c2c, 0x9c50, 0x9c70,
    0x9d14, 0x9d2c, 0x9d44, 0x9d5c, 0x9d64, 0x9da0, 0x9ed4, 0xa03c,
    0xa174, 0xa178, 0xa180, 0xa240, 0xc67c, 0xc6b0, 0xc7a8, 0xc7bc,
    0xc9ec, 0xca4c, 0xcc40, 0xcc50, 0xd71c
#elif (defined(KI2_L11))
    0x0178, 0x02ec, 0x0474, 0x04bc, 0x0b14, 0x0b18, 0x0bd0, 0x0bdc,
    0x0be0, 0x1750, 0x176c, 0x1960, 0x1af8, 0x1bc4, 0x1c04, 0x1dd4,
    0x1e40, 0x2044, 0x20f8, 0x2634, 0x27b8, 0x27bc, 0x3130, 0x3134,
    0x3204, 0x3208, 0x33dc, 0x388c, 0x3d00, 0x3d98, 0x3db8, 0x3dd8,
    0x3e6c, 0x3e90, 0x3ee8, 0x3f0c, 0x5d20, 0x5d28, 0x61fc, 0x6708,
    0x6858, 0x6880, 0x68e8, 0x68fc, 0x94bc, 0x94c0, 0x9574, 0x9b48,
    0x9b90, 0x9bac, 0x9cec, 0x9d00, 0x9f74, 0x9fa0, 0x9fc4, 0x9fe4,
    0xa088, 0xa0a0, 0xa0b8, 0xa0d0, 0xa0d8, 0xa114, 0xa248, 0xa3b0,
    0xa4e8, 0xa4ec, 0xa4f4, 0xa5b4, 0xc9f0, 0xca24, 0xcb1c, 0xcb30,
    0xcd60, 0xcdc0, 0xcfb4, 0xcfc4, 0xda8c
#endif
#elif defined(KI_BOARD_20351)
#if defined(KI_L13)
    0x0208, 0x020c, 0x02ac, 0x02b0, 0x0428, 0x0454, 0x0470, 0x08d0,
    0x312c, 0x317c, 0x31a0, 0x344c, 0x3450, 0x4920, 0x493c, 0x49e8,
    0x4a00, 0x4a18, 0x4a30, 0x4a38, 0x4a74, 0x4b78, 0x4b90, 0x4bec,
    0x4c2c, 0x5aec, 0x5af0, 0x62ec, 0x6760, 0x67f8, 0x6818, 0x6838,
    0x68cc, 0x68f0, 0x6948, 0x696c, 0x6ba4, 0x6f94, 0x7420, 0x7514,
    0x7a74, 0x7a94, 0x7bd4, 0x7cec, 0x9d84, 0xa6b0, 0xa74c, 0xa750,
    0xa8d4, 0xa8d8, 0xbdc8, 0xbeec, 0xc010, 0xc014, 0xc01c, 0xc158,
    0xc188, 0xc5b4, 0xc648, 0xc6d0, 0xd350, 0xd404, 0xd430, 0xd458,
    0xd480
#elif defined(KI_L14)
    0x0218, 0x021c, 0x02bc, 0x02c0, 0x0438, 0x0464, 0x0480, 0x08e0,
    0x313c, 0x318c, 0x31b0, 0x345c, 0x3460, 0x4930, 0x494c, 0x49f8,
    0x4a10, 0x4a28, 0x4a40, 0x4a48, 0x4a84, 0x4b88, 0x4ba0, 0x4bfc,
    0x4c3c, 0x5afc, 0x5b00, 0x62fc, 0x6770, 0x6808, 0x6828, 0x6848,
    0x68dc, 0x6900, 0x6958, 0x697c, 0x6bb4, 0x6fa4, 0x7430, 0x7524,
    0x7a84, 0x7aa4, 0x7be4, 0x7cfc, 0x9d94, 0xa6c8, 0xa764, 0xa768,
    0xa8ec, 0xa8f0, 0xbde0, 0xbf04, 0xc028, 0xc02c, 0xc034, 0xc170,
    0xc1a0, 0xc5cc, 0xc660, 0xc6e8, 0xd370, 0xd424, 0xd450, 0xd478,
    0xd4a0
#elif defined(KI_L15D)
    0x04f8, 0x04fc, 0x059c, 0x05a0, 0x0718, 0x0744, 0x0760, 0x0bc0,
    0x3438, 0x3488, 0x34ac, 0x3758, 0x375c, 0x4c54, 0x4c70, 0x4d1c,
    0x4d34, 0x4d4c, 0x4d64, 0x4d6c, 0x4da8, 0x4eac, 0x4ec4, 0x4f20,
    0x4f60, 0x5e28, 0x5e2c, 0x663c, 0x6ab0, 0x6b48, 0x6b68, 0x6b88,
    0x6c1c, 0x6c40, 0x6c98, 0x6cbc, 0x6ef4, 0x72e4, 0x7770, 0x7864,
    0x7dd4, 0x7df4, 0x7f34, 0x804c, 0xa49c, 0xadd8, 0xae74, 0xae78,
    0xaffc, 0xb000, 0xc584, 0xc6a8, 0xc7cc, 0xc7d0, 0xc7d8, 0xc914,
    0xc944, 0xcd70, 0xce04, 0xce8c, 0xdb10, 0xdbc4, 0xdbf0, 0xdc18,
    0xdc40
#elif defined(KI_L15DI)
    0x03e8, 0x03ec, 0x048c, 0x0490, 0x0608, 0x0634, 0x0650, 0x0ab0,
    0x3328, 0x3378, 0x339c, 0x3648, 0x364c, 0x4b44, 0x4b60, 0x4c0c,
    0x4c24, 0x4c3c, 0x4c54, 0x4c5c, 0x4c98, 0x4d9c, 0x4db4, 0x4e10,
    0x4e50, 0x5d18, 0x5d1c, 0x652c, 0x69a0, 0x6a38, 0x6a58, 0x6a78,
    0x6b0c, 0x6b30, 0x6b88, 0x6bac, 0x6de4, 0x71d4, 0x7660, 0x7754,
    0x7cc4, 0x7ce4, 0x7e24, 0x7f3c, 0xa38c, 0xacc8, 0xad64, 0xad68,
    0xaeec, 0xaef0, 0xc474, 0xc598, 0xc6bc, 0xc6c0, 0xc6c8, 0xc804,
    0xc834, 0xcc60, 0xccf4, 0xcd7c, 0xda00, 0xdab4, 0xdae0, 0xdb08,
    0xdb30
#endif
#endif
};

// 0x88030000 + offset
static uint16_t offsets_3[] = {
#if defined(KI_BOARD_19489)
#if defined(KI2_L10)
    0x7038, 0x7058, 0x7078, 0x7098, 0x70b8
#elif (defined(KI2_L11))
    0x7438, 0x7458, 0x7478, 0x7498, 0x74b8
#endif
#elif defined(KI_BOARD_20351)
#if defined(KI_L13)
    0xbfd4, 0xc008, 0xc45c, 0xc47c, 0xc49c, 0xc4bc, 0xc4dc
#elif defined(KI_L14)
    0xbfe4, 0xc018, 0xc46c, 0xc48c, 0xc4ac, 0xc4cc, 0xc4ec
#elif defined(KI_L15D)
    0xc014, 0xc048, 0xc4cc, 0xc4ec, 0xc50c, 0xc52c, 0xc54c
#elif defined(KI_L15DI)
    0xc4cc, 0xc4ec, 0xc50c, 0xc52c, 0xc54c, 0xc014, 0xc048
#endif
#endif
};

static uint16_t *offsets[4] = {
    offsets_0,
    offsets_1,
    offsets_2,
    offsets_3,
};

static constexpr uint8_t offset_count[] = {
    sizeof(offsets_0) / sizeof(uint16_t),
    sizeof(offsets_1) / sizeof(uint16_t),
    sizeof(offsets_2) / sizeof(uint16_t),
    sizeof(offsets_3) / sizeof(uint16_t),
};

[[maybe_unused]] static void apply(void)
{
#if defined(KI_BOARD_20351)
#if defined(KI_L15D) || defined(KI_L15DI)
    *(uint16_t *)0x8800827c = 0xfff8;
#elif defined(KI_L14)
    *(uint16_t *)0x8800804c = 0xfff8;
#elif defined(KI_L13)
    *(uint16_t *)0x88008048 = 0xfff8;
#endif
#endif

    uint8_t *org = (uint8_t *)0x88000000;
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t *base = org + (i << 16);
        for (uint8_t j = 0; j < offset_count[i]; j++)
        {
            uint8_t *ptr = base + offsets[i][j];
            *ptr = mapping[(*ptr - 0x80) >> 3];
        }
    }
}

#if (defined(KI_BOARD_20351) && (defined(KI_L15DI) || defined(KI_L15D) || defined(KI_L14) || defined(KI_L13))) || (defined(KI_BOARD_19489) && (defined(KI2_L10) || defined(KI2_L11)))
patch_t patch_kix_remap = {
    .name = "{PATCH} U96 I/O remap",
    .apply = apply,
    .status = false,
};
#endif
