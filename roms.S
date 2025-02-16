# SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
# SPDX-License-Identifier: AGPL-3.0-only
 
#define XSTR(s) STR(s)
#define STR(s) #s

.section .rodata

.align 8
.globl zroms
zroms:
    .incbin XSTR(build/roms/ZROM-0.addr)
    .dc.l zrom_0_data
    .incbin XSTR(build/roms/ZROM-1.addr)
    .dc.l zrom_1_data
    .incbin XSTR(build/roms/ZROM-2.addr)
    .dc.l zrom_2_data

.section .assets

.align 8
zrom_0_data: .incbin XSTR(build/roms/ZROM-0.zbin)
.align 8
zrom_1_data: .incbin XSTR(build/roms/ZROM-1.zbin)
.align 8
zrom_2_data: .incbin XSTR(build/roms/ZROM-2.zbin)

.text

.set noat
.set noreorder
.set nomacro
.align 8
.globl rom_start
rom_start:
    lui $ra,0x8800
    dmtc1 $zero,$0
    dmtc1 $zero,$1
    dmtc1 $zero,$2
    dmtc1 $zero,$3
    dmtc1 $zero,$4
    dmtc1 $zero,$5
    dmtc1 $zero,$6
    dmtc1 $zero,$7
    dmtc1 $zero,$8
    dmtc1 $zero,$9
    dmtc1 $zero,$10
    dmtc1 $zero,$11
    dmtc1 $zero,$12
    dmtc1 $zero,$13
    dmtc1 $zero,$14
    dmtc1 $zero,$15
    dmtc1 $zero,$16
    dmtc1 $zero,$17
    dmtc1 $zero,$18
    dmtc1 $zero,$19
    dmtc1 $zero,$20
    dmtc1 $zero,$21
    dmtc1 $zero,$22
    dmtc1 $zero,$23
    dmtc1 $zero,$24
    dmtc1 $zero,$25
    dmtc1 $zero,$26
    dmtc1 $zero,$27
    dmtc1 $zero,$28
    dmtc1 $zero,$29
    dmtc1 $zero,$30
    dmtc1 $zero,$31
    or $at,$zero,$zero
    or $v0,$zero,$zero
    or $v1,$zero,$zero
    or $a0,$zero,$zero
    or $a1,$zero,$zero
    or $a2,$zero,$zero
    or $a3,$zero,$zero
    or $t0,$zero,$zero
    or $t1,$zero,$zero
    or $t2,$zero,$zero
    or $t3,$zero,$zero
    or $t4,$zero,$zero
    or $t5,$zero,$zero
    or $t6,$zero,$zero
    or $t7,$zero,$zero
    or $s0,$zero,$zero
    or $s1,$zero,$zero
    or $s2,$zero,$zero
    or $s3,$zero,$zero
    or $s4,$zero,$zero
    or $s5,$zero,$zero
    or $s6,$zero,$zero
    or $s7,$zero,$zero
    or $t8,$zero,$zero
    or $t9,$zero,$zero
    or $k0,$zero,$zero
    or $k1,$zero,$zero
    or $gp,$zero,$zero
    or $sp,$zero,$zero
    or $fp,$zero,$zero
    jr $ra
    nop
