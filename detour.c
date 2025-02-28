/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "detour.h"
#include "asm.h"

/*
 * The is a rudimentary implementation, you wil have to take care of not fucking up the program flow !
 * Also you have to make sure you are not stealing an instruction with a delay slot; or :explosion: !
 * You are warned, if you still decide to use it, prepare for unforseen consequences ;)
 */
void *detour(uint32_t *gateway, uint32_t *src, void *dst)
{
    // Setup gateway
    gateway[0] = src[0]; // copy stolen instructions
    gateway[1] = src[1];
    gateway[2] = J(REL_ADDR(src + 2)); // jump to origin function after stolen instructions
    gateway[3] = NOP();

    // Detour
    src[0] = J(REL_ADDR(dst));
    src[1] = NOP();

    return gateway;
}

DETOUR_FN void detour_save_registers(void)
{
    asm volatile(
        ".set noat\n"
        ".set noreorder\n"
        "addiu $sp,$sp,-0xF0\n"
        "sd $v0,0x0($sp)\n"
        "sd $v1,0x8($sp)\n"
        "sd $a0,0x10($sp)\n"
        "sd $a1,0x18($sp)\n"
        "sd $a2,0x20($sp)\n"
        "sd $a3,0x28($sp)\n"
        "sd $t0,0x30($sp)\n"
        "sd $t1,0x38($sp)\n"
        "sd $t2,0x40($sp)\n"
        "sd $t3,0x48($sp)\n"
        "sd $t4,0x50($sp)\n"
        "sd $t5,0x58($sp)\n"
        "sd $t6,0x60($sp)\n"
        "sd $t7,0x68($sp)\n"
        "sd $t8,0x70($sp)\n"
        "sd $t9,0x78($sp)\n"
        "sd $s0,0x88($sp)\n"
        "sd $s1,0x90($sp)\n"
        "sd $s2,0x98($sp)\n"
        "sd $s3,0xA0($sp)\n"
        "sd $s4,0xA8($sp)\n"
        "sd $s5,0xB0($sp)\n"
        "sd $s6,0xB8($sp)\n"
        "sd $s7,0xC0($sp)\n"
        "sd $s8,0xC8($sp)\n"
        "sd $k0,0xD0($sp)\n"
        "sd $k1,0xD8($sp)\n"
        "sd $fp,0xE0($sp)\n");
}

DETOUR_FN void detour_load_registers(void)
{
    asm volatile(
        ".set noat\n"
        ".set noreorder\n"
        "ld $v0,0x0($sp)\n"
        "ld $v1,0x8($sp)\n"
        "ld $a0,0x10($sp)\n"
        "ld $a1,0x18($sp)\n"
        "ld $a2,0x20($sp)\n"
        "ld $a3,0x28($sp)\n"
        "ld $t0,0x30($sp)\n"
        "ld $t1,0x38($sp)\n"
        "ld $t2,0x40($sp)\n"
        "ld $t3,0x48($sp)\n"
        "ld $t4,0x50($sp)\n"
        "ld $t5,0x58($sp)\n"
        "ld $t6,0x60($sp)\n"
        "ld $t7,0x68($sp)\n"
        "ld $t8,0x70($sp)\n"
        "ld $t9,0x78($sp)\n"
        "ld $s0,0x88($sp)\n"
        "ld $s1,0x90($sp)\n"
        "ld $s2,0x98($sp)\n"
        "ld $s3,0xA0($sp)\n"
        "ld $s4,0xA8($sp)\n"
        "ld $s5,0xB0($sp)\n"
        "ld $s6,0xB8($sp)\n"
        "ld $s7,0xC0($sp)\n"
        "ld $s8,0xC8($sp)\n"
        "ld $k0,0xD0($sp)\n"
        "ld $k1,0xD8($sp)\n"
        "ld $fp,0xE0($sp)\n"
        "addiu $sp,$sp,0xF0\n");
}
