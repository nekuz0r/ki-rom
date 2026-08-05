#pragma once
#ifndef _CACHE_H_
#define _CACHE_H_

/*
 * Both of these need the "memory" clobber. Without it GCC assumes the asm
 * does not touch memory and is free to move surrounding loads and stores
 * across it -- volatile only stops the asm itself from being deleted or
 * duplicated. A cache writeback that the compiler has sunk the stores past
 * is a no-op.
 *
 * Wrapped in do/while(0) so they behave as single statements.
 */
#define CACHE_OP(op, base, offset)                                                            \
    do                                                                                        \
    {                                                                                         \
        asm volatile("cache %0, %2(%1)" : : "i"(op), "r"(base), "i"(offset) : "memory");      \
    } while (0)

#define SYNC()                                \
    do                                        \
    {                                         \
        asm volatile("sync" : : : "memory");  \
    } while (0)

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
#define HIT_WRITEBACK_D ((6 << 2) | DCACHE)

// Primary Instruction Cache Operations
#define INDEX_INVALIDATE_I ((0 << 2) | ICACHE)
#define INDEX_LOAD_TAG_I ((1 << 2) | ICACHE)
#define INDEX_STORE_TAG_I ((2 << 2) | ICACHE)
#define HIT_INVALIDATE_I ((4 << 2) | ICACHE)
#define FILL_I ((5 << 2) | ICACHE)
#define HIT_WRITEBACK_I ((6 << 2) | ICACHE)

#endif
