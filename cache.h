#pragma once
#ifndef _CACHE_H_
#define _CACHE_H_

#define CACHE_OP(op, base, offset) asm volatile("cache %0, %2(%1)" : : "i"(op), "r"(base), "i"(offset));
#define SYNC() asm volatile("sync");

#define ICACHE 0
#define DCACHE 1

#define CACHE_LINE_SIZE 32

// Primary Data Cache Operations
#define INDEX_WRITEBACK_INVALIDATE_D ((0 << 2) | DCACHE)
#define INDEX_LOAD_TAG_D ((1 << 2) | DCACHE)
#define INDEX_STORE_TAG_D ((2 << 2) | DCACHE)
#define CREATE_DIRTY_EXCLUSIVE_D ((3 << 2) | DCACHE)
#define HIT_INVALIDATE_D ((4 << 2) | DCACHE)
#define HIT_WRITEBACK_INVALIDATE_D ((5 << 2) | DCACHE)
#define HIT_WRITEBACK_D ((7 << 2) | DCACHE)

// Primary Instruction Cache Operations
#define INDEX_INVALIDATE_I ((0 << 2) | ICACHE)
#define INDEX_LOAD_TAG_I ((1 << 2) | ICACHE)
#define INDEX_STORE_TAG_I ((2 << 2) | ICACHE)
#define HIT_INVALIDATE_I ((4 << 2) | ICACHE)
#define FILL_I ((5 << 2) | ICACHE)
#define HIT_WRITEBACK_I ((6 << 2) | ICACHE)

#endif
