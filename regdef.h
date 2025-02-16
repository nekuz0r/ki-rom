/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _REGDEF_H_
#define _REGDEF_H_

#define CP0_INDEX $0
#define CP0_RANDOM $1
#define CP0_ENTRY_LO_0 $2
#define CP0_ENTRY_LO_1 $3
#define CP0_CONTEXT $4
#define CP0_PAGE_MASK $5
#define CP0_WIRED $6
#define CP0_BAD_V_ADDR $8
#define CP0_COUNT $9
#define CP0_ENTRY_HI $10
#define CP0_COMPARE $11
#define CP0_STATUS $12
#define CP0_CAUSE $13
#define CP0_EPC $14
#define CP0_PRID $15
#define CP0_CONFIG $16
#define CP0_LLADDR $17
#define CP0_XCONTEXT $20
#define CP0_ECC $26
#define CP0_CACHE_ERR $27
#define CP0_TAG_LO $28
#define CP0_TAG_HI $29
#define CP0_ERROR_EPC $30

#define CP1_STATUS $31

#define zero $0
#define at $1
#define v0 $2
#define v1 $3
#define a0 $4
#define a1 $5
#define a2 $6
#define a3 $7
#define t0 $8
#define t1 $9
#define t2 $10
#define t3 $11
#define t4 $12
#define t5 $13
#define t6 $14
#define t7 $15
#define s0 $16
#define s1 $17
#define s2 $18
#define s3 $19
#define s4 $20
#define s5 $21
#define s6 $22
#define s7 $23
#define t8 $24
#define t9 $25
#define k0 $26
#define k1 $27
#define gp $28
#define sp $29
#define fp $30
#define ra $31

#endif
