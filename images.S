# SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
# SPDX-License-Identifier: AGPL-3.0-only

.macro .inczimg img
.align 8
.globl zasset_\img
zasset_\img: .incbin "build/assets/images/\img\.zbin"
.endm

.section .assets
.inczimg logo
.inczimg ki1_cinder
.inczimg ki1_combo
.inczimg ki1_eyedol
.inczimg ki1_fulgore
.inczimg ki1_glacius
.inczimg ki1_jago
.inczimg ki1_orchid
.inczimg ki1_riptor
.inczimg ki1_sabrewulf
.inczimg ki1_spinal
.inczimg ki1_thunder
.inczimg ki2_combo
.inczimg ki2_fulgore
.inczimg ki2_gargos
.inczimg ki2_glacius
.inczimg ki2_jago
.inczimg ki2_kim_wu
.inczimg ki2_maya
.inczimg ki2_orchid
.inczimg ki2_sabrewulf
.inczimg ki2_spinal
.inczimg ki2_tusk
.inczimg rare_logo
.inczimg nintendo_logo
