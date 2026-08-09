# Killer Instinct MIPS Code Compression Algorithm

This document describes the custom compression algorithm used in the Killer Instinct arcade game's bootrom to store MIPS R4000 executable code. The algorithm was reverse-engineered from the original ROM and reimplemented in the `unpack.c` (decompressor) and `pack.c` (compressor) tools.

## Table of Contents

1. [Overview](#overview)
2. [File Format](#file-format)
3. [MIPS Instruction Encoding](#mips-instruction-encoding)
4. [Huffman Coding](#huffman-coding)
5. [Move-to-End Transform](#move-to-end-transform)
6. [Format Flags](#format-flags)
7. [Bitstream Format](#bitstream-format)
8. [Decompression Process](#decompression-process)
9. [Compression Process](#compression-process)
10. [Lookup Tables](#lookup-tables)

---

## Overview

The Killer Instinct compression algorithm is specifically designed for MIPS R4000 machine code. It achieves compression by:

1. **Separating instruction fields** - Breaking each 32-bit instruction into its component fields (opcode, registers, immediates)
2. **Huffman coding** - Using 24 different Huffman tables optimized for each field type
3. **Move-to-End (MTE) transform** - Exploiting register locality by tracking recently used registers
4. **Raw-bit fallback** - Emitting reserved or unencodable instructions as raw bits (20-bit for reserved SPECIAL, 21-bit for CO-format coprocessor, 26-bit for reserved opcodes) instead of through a Huffman table

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Compression Pipeline                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   MIPS Instructions                                                  │
│         │                                                            │
│         ▼                                                            │
│   ┌───────────────┐                                                  │
│   │ Field         │  Split into opcode, rs, rt, rd, shamt, funct,   │
│   │ Separation    │  immediate, or target fields                     │
│   └───────┬───────┘                                                  │
│           │                                                          │
│           ▼                                                          │
│   ┌───────────────┐                                                  │
│   │ Move-to-End   │  Transform register values to MTE indices        │
│   │ Transform     │  (recently used registers get lower indices)     │
│   └───────┬───────┘                                                  │
│           │                                                          │
│           ▼                                                          │
│   ┌───────────────┐                                                  │
│   │ Huffman       │  Encode each field using the appropriate         │
│   │ Encoding      │  Huffman table (24 tables total)                 │
│   └───────┬───────┘                                                  │
│           │                                                          │
│           ▼                                                          │
│   Compressed Bitstream                                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## File Format

### Compressed ROM Layout

```
┌────────────────────────────────────────────────────────────────┐
│ Offset 0x0000: Function Code Table (128 bytes)                 │
│                64 entries × 2 bytes, little-endian             │
├────────────────────────────────────────────────────────────────┤
│ Offset 0x0080: REGIMM Table (64 bytes)                         │
│                32 entries × 2 bytes, little-endian             │
├────────────────────────────────────────────────────────────────┤
│ Offset 0x00C0: Extended Table (64 bytes)                       │
│                32 entries × 2 bytes, little-endian             │
├────────────────────────────────────────────────────────────────┤
│ Offset 0x0100: Opcode Table (128 bytes)                        │
│                64 entries × 2 bytes, little-endian             │
├────────────────────────────────────────────────────────────────┤
│ Offset 0x0180: Compressed Data Stream                          │
│                                                                │
│   ┌──────────────────────────────────────────────────────┐     │
│   │ Header:                                              │     │
│   │   - Magic "br" (16 bits) = 0x7262                    │     │
│   │   - File count (8 bits)                              │     │
│   └──────────────────────────────────────────────────────┘     │
│   ┌──────────────────────────────────────────────────────┐     │
│   │ For each file:                                       │     │
│   │   - Type (8 bits) = 0 for code                       │     │
│   │   - Decompressed size (32 bits)                      │     │
│   │   - Load address (32 bits, KSEG0 virtual)            │     │
│   │   - 24 Huffman tables (byte-aligned)                 │     │
│   │   - Compressed instruction stream                    │     │
│   └──────────────────────────────────────────────────────┘     │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Original KI ROM Offsets

| ROM Version | Funct Table | REGIMM Table | Extended Table | Opcode Table | Compressed Data |
|-------------|-------------|--------------|----------------|--------------|-----------------|
| KI1         | 0x0E48      | 0x0EC8       | 0x0F08         | 0x0F48       | 0x0FD0          |
| KI1 p47     | 0x0E80      | 0x0F00       | 0x0F40         | 0x0F80       | 0x1010          |
| KI2         | 0x0EB0      | 0x0F30       | 0x0F70         | 0x0FB0       | 0x1040          |

---

## MIPS Instruction Encoding

MIPS R4000 instructions are 32 bits and come in three formats:

### R-Type (Register)

Used for arithmetic and logical operations between registers.

```
  31      26 25    21 20    16 15    11 10     6 5        0
 ┌─────────┬────────┬────────┬────────┬────────┬──────────┐
 │  opcode │   rs   │   rt   │   rd   │ shamt  │  funct   │
 │ (6 bits)│(5 bits)│(5 bits)│(5 bits)│(5 bits)│ (6 bits) │
 └─────────┴────────┴────────┴────────┴────────┴──────────┘

 opcode: Operation code (always 0x00 for SPECIAL instructions)
 rs:     First source register
 rt:     Second source register
 rd:     Destination register
 shamt:  Shift amount (for shift instructions)
 funct:  Function code (determines the actual operation)
```

**Examples:**
- `ADD $t0, $s0, $s1` - Add registers
- `SLL $t0, $s0, 4` - Shift left logical
- `JR $ra` - Jump to return address (0x03E00008)

### I-Type (Immediate)

Used for operations with immediate values, loads, stores, and branches.

```
  31      26 25    21 20    16 15                        0
 ┌─────────┬────────┬────────┬────────────────────────────┐
 │  opcode │   rs   │   rt   │        immediate           │
 │ (6 bits)│(5 bits)│(5 bits)│        (16 bits)           │
 └─────────┴────────┴────────┴────────────────────────────┘

 opcode:    Operation code
 rs:        Source/base register
 rt:        Destination register or branch condition
 immediate: 16-bit signed/unsigned value or branch offset
```

**Examples:**
- `ADDIU $t0, $s0, 100` - Add immediate unsigned
- `LW $t0, 0x10($sp)` - Load word from memory
- `BEQ $t0, $t1, label` - Branch if equal

### J-Type (Jump)

Used for unconditional jumps.

```
  31      26 25                                          0
 ┌─────────┬──────────────────────────────────────────────┐
 │  opcode │                   target                     │
 │ (6 bits)│                  (26 bits)                   │
 └─────────┴──────────────────────────────────────────────┘

 opcode: Operation code (0x02 for J, 0x03 for JAL)
 target: 26-bit word-aligned address (shifted left 2 bits)
```

**Examples:**
- `J 0x80001000` - Jump to address
- `JAL function` - Jump and link (function call)

---

## Huffman Coding

### Skip-List Tree Format

The Huffman tables are serialized in a skip-list format that allows efficient traversal without recursion. Each node in the tree is encoded as:

```
┌─────────────────────────────────────────────────────────────────┐
│                     Huffman Node Encoding                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Terminal Node (leaf):                                           │
│  ┌──────────┬──────────┐                                         │
│  │   0x00   │  symbol  │                                         │
│  │ (1 byte) │ (1 byte) │                                         │
│  └──────────┴──────────┘                                         │
│                                                                  │
│  Internal Node (skip count < 128):                               │
│  ┌──────────────────┐                                            │
│  │   skip_count     │  1-127: bytes to skip for left branch      │
│  │    (1 byte)      │                                            │
│  └──────────────────┘                                            │
│                                                                  │
│  Internal Node (skip count >= 128):                              │
│  ┌──────────────────┬──────────────────┐                         │
│  │ 0x80 | (hi >> 8) │    low_byte      │  Extended 15-bit skip   │
│  │    (1 byte)      │    (1 byte)      │                         │
│  └──────────────────┴──────────────────┘                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Tree Navigation

During decoding, the tree is navigated using bits from the bitstream:

- **Bit = 1**: Take the right branch (continue to next sequential entry)
- **Bit = 0**: Take the left branch (skip forward by the skip count)

```
Example Tree for symbols A(freq=4), B(freq=2), C(freq=1), D(freq=1):

                    [root]
                   /      \
                  /        \
               [A]         [internal]
              (0)          /        \
                          /          \
                       [B]          [internal]
                       (10)         /        \
                                   /          \
                                [C]          [D]
                               (110)        (111)

Serialized (skip-list format):
  Offset 0: skip=6  (skip to A if bit=0, else continue to right subtree)
  Offset 1: skip=4  (skip to B if bit=0, else continue)
  Offset 2: skip=2  (skip to C if bit=0, else continue to D)
  Offset 3: 0x00    (terminal marker)
  Offset 4: 'D'     (symbol D, code 111)
  Offset 5: 0x00    (terminal marker)
  Offset 6: 'C'     (symbol C, code 110)
  Offset 7: 0x00    (terminal marker)
  Offset 8: 'B'     (symbol B, code 10)
  Offset 9: 0x00    (terminal marker)
  Offset 10: 'A'    (symbol A, code 0)
```

### The 24 Huffman Tables

| Index | Offset | Purpose | Typical Symbols |
|-------|--------|---------|-----------------|
| 0 | 0x40 | Opcode | 0-63 (MIPS opcodes) |
| 1 | 0x44 | Function code | 0-63 (SPECIAL funct) |
| 2 | 0x48 | REGIMM rt | 0-31 (branch types) |
| 3 | 0x4C | rs (MTF table 1) | 0-31 (MTE indices) |
| 4 | 0x50 | rt (MTF table 1) | 0-31 (MTE indices) |
| 5 | 0x54 | rd (MTF table 1) | 0-31 (MTE indices) |
| 6 | 0x58 | shamt (specialized) | 0-31 (shift amounts) |
| 7 | 0x5C | rs (MTF table 2) | 0-31 (MTE indices) |
| 8 | 0x60 | Direct values | 0-31 (raw register/field values) |
| 9 | 0x64 | Immediate high (type 1) | 0-255 (fallback/REGIMM) |
| 10 | 0x68 | Immediate low (type 1) | 0-255 (fallback/REGIMM) |
| 11 | 0x6C | Jump target byte 0 | 0-63 (bits 20-25) |
| 12 | 0x70 | Jump target byte 1 | 0-63 (bits 14-19) |
| 13 | 0x74 | Jump target byte 2 | 0-127 (bits 7-13) |
| 14 | 0x78 | Jump target byte 3 | 0-127 (bits 0-6) |
| 15 | 0x7C | Immediate high (type 2) | 0-255 (load/store/ALU) |
| 16 | 0x80 | Immediate low (type 2) | 0-255 (load/store/ALU) |
| 17 | 0x84 | Immediate high (type 3) | 0-255 (ALU operations) |
| 18 | 0x88 | Immediate low (type 3) | 0-255 (ALU operations) |
| 19 | 0x8C | Immediate high (type 4) | 0-255 (branches) |
| 20 | 0x90 | Immediate low (type 4) | 0-255 (branches) |
| 21 | 0x94 | rs (direct) | 0-31 (for special cases) |
| 22 | 0x98 | Extended opcode | 0-31 (COP rs field) |
| 23 | 0x9C | COP rt | 0-31 (coprocessor branch) |

---

## Move-to-End Transform

The Move-to-End (MTE) transform exploits the locality of register usage in MIPS code. Recently used registers are more likely to be used again soon, so they're assigned smaller indices.

### Initial State

Both MTE tables start with values `[31, 30, 29, ..., 1, 0]`:

```
Table: [31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0]
Index:   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
        16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
```

### Transform Operation

When encoding register value V:
1. Find V's position (index) in the table
2. Output the index
3. Shift all elements from that position to the end left by one
4. Place V at position 31 (the end)

```
Example: Encoding register $t0 (value 8), then $s0 (value 16), then $t0 again

Initial table:
  [31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
   15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0]

Step 1: Encode $t0 (value 8)
  - Find 8 at index 23
  - Output: 23
  - Shift elements 23→30 left
  - Place 8 at position 31

After Step 1:
  [31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
   15, 14, 13, 12, 11, 10,  9,  7,  6,  5,  4,  3,  2,  1,  0,  8]
                                    └──────────────────────────┘
                                    Elements shifted left, 8 at end

Step 2: Encode $s0 (value 16)
  - Find 16 at index 15
  - Output: 15
  - Shift elements 15→30 left
  - Place 16 at position 31

After Step 2:
  [31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
   15, 14, 13, 12, 11, 10,  9,  7,  6,  5,  4,  3,  2,  1,  0,  8, 16]
                                                                   └──┘
                                                               16 now at end

Step 3: Encode $t0 (value 8) again
  - Find 8 at index 30 (it moved closer to end!)
  - Output: 30
  - Only one element to shift
  - Place 8 at position 31

After Step 3:
  [31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
   15, 14, 13, 12, 11, 10,  9,  7,  6,  5,  4,  3,  2,  1,  0, 16,  8]

Notice: Second occurrence of $t0 encoded as 30 instead of 23!
        High indices (near 31) represent recently used registers.
```

### Why "Move-to-End"?

The term "Move-to-Front" (MTF) is commonly used in compression literature, where recently used symbols move to index 0. However, the KI algorithm actually moves symbols to the **end** (index 31) of the table. We call this "Move-to-End" (MTE) to accurately describe the behavior.

The Huffman tables for MTE indices are optimized with:
- Higher indices (near 31) → shorter Huffman codes (recently used)
- Lower indices (near 0) → longer Huffman codes (rarely used)

---

## Format Flags

Format flags are 16-bit values that define how each instruction type is encoded. They're stored in four lookup tables indexed by opcode, function code, REGIMM type, or extended opcode.

### Flag Bit Definitions

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Format Flags (16 bits)                          │
├──────┬──────────────────────────────────────────────────────────────┤
│ Bit  │ Meaning                                                       │
├──────┼──────────────────────────────────────────────────────────────┤
│ 0x0001│ rs: Encode using MTF table 1                                 │
│ 0x0002│ rs: Encode directly (no MTF)                                 │
│ 0x0004│ rt: Encode using MTF table 1                                 │
│ 0x0008│ rt: Encode directly (no MTF)                                 │
│ 0x0010│ rd: Encode using MTF table 1                                 │
│ 0x0020│ rd: Encode directly (no MTF)                                 │
│ 0x0040│ shamt: Encode using specialized Huffman table                │
│ 0x0080│ shamt: Encode directly                                       │
│ 0x0100│ Jump target: Split into 4 parts (6+6+7+7 bits)               │
│ 0x0200│ rs: Use MTF table 2 instead of table 1 (with 0x0001)         │
│ 0x0400│ Immediate type 1: Load/store offset encoding                 │
│ 0x0800│ Immediate type 2: ALU immediate encoding                     │
│ 0x0C00│ Immediate type 3: Branch offset encoding                     │
│ 0x1000│ Immediate: Single byte direct encoding                       │
│ 0x2000│ Special: NOP instruction (no fields)                         │
│ 0x4000│ Special: JR $ra instruction (no fields)                      │
│ 0xFFFF│ Extended format: Need additional lookup                      │
└──────┴──────────────────────────────────────────────────────────────┘
```

### Example Format Flags

| Instruction | Opcode | Format Flags | Meaning |
|-------------|--------|--------------|---------|
| ADD | 0 (funct=32) | 0x0095 | rs:MTF1, rt:MTF1, rd:MTF1 |
| SLL | 0 (funct=0) | 0x0056 | rt:MTF1, rd:MTF1, shamt:huffman |
| LW | 35 | 0x0605 | rs:MTF2, rt:MTF1, imm:type1 |
| BEQ | 4 | 0x0C05 | rs:MTF1, rt:MTF1, imm:type3 |
| J | 2 | 0x0100 | jump target (26-bit split) |
| JR $ra | 0 (funct=8) | 0x00A9 | rs:MTF1 — no special case, see below |
| NOP | 0 (funct=0) | 0x0056 | rt:MTF1, rd:MTF1, shamt:huffman |
| COP1 | 17 | 0xFFFF | Extended format (use rs for lookup) |

**0x2000 and 0x4000 are defined by the format but unused by every shipped
table.** No entry in any of the four lookup tables holds either value, so
`jr $ra` goes down the ordinary `0x00A9` path — its `rs` is MTF1-encoded like
any other `jr` — and `nop` goes down the ordinary `0x0056` SLL path, with all
its fields encoded. Treating `jr $ra` as encoded by opcode and funct alone
desynchronises the MTF state between the two compression passes; that was a
real bug, fixed in 321d0ce.

The decompressor still carries both branches, because it dispatches on table
data read out of the ROM image at runtime and must stay byte-compatible with a
table that does use them. The compressor's copies are dead for good, since it
builds from its own compile-time tables.

---

## Bitstream Format

### Reading Bits (LSB-First)

The bitstream is read least-significant-bit first using a 64-bit buffer:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Bitstream Buffer Architecture                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Memory Layout (bytes at addresses N, N+1, N+2, ...):                │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐                │
│  │ B0 │ B1 │ B2 │ B3 │ B4 │ B5 │ B6 │ B7 │ B8 │... │                │
│  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘                │
│                                                                      │
│  64-bit Buffer (little-endian load):                                 │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │ B7 │ B6 │ B5 │ B4 │ B3 │ B2 │ B1 │ B0 │                     │     │
│  └─────────────────────────────────────────────────────────────┘     │
│    MSB ◄──────────────────────────────────────────────────► LSB      │
│                                                                      │
│  Bits are extracted from LSB side:                                   │
│  - Read 3 bits: result = buffer & 0x7, buffer >>= 3                  │
│  - Read 5 bits: result = buffer & 0x1F, buffer >>= 5                 │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Buffer Refill Strategy

The reader maintains a 64-bit buffer with a "bits remaining" counter:

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Buffer Refill Mechanism                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  State Variables:                                                    │
│  - bit_buffer:     64-bit buffer holding prefetched bits             │
│  - stream_ptr:     Points 7 bytes ahead of current read position     │
│  - bits_remaining: Negative count (-56 when full, 0 when empty)      │
│                                                                      │
│  Refill Logic:                                                       │
│  1. Read next 64 bits from stream_ptr (lookahead)                    │
│  2. Shift lookahead left by (-bits_remaining) positions              │
│  3. OR into bit_buffer (merge bits)                                  │
│  4. After consuming N bits: bits_remaining += N                      │
│  5. If bits_remaining > 0: need refill                               │
│     - Advance stream_ptr by 7 bytes                                  │
│     - Reload buffer from lookahead                                   │
│     - bits_remaining -= 56                                           │
│                                                                      │
│  Why 7 bytes (56 bits)?                                              │
│  - 64-bit reads overlap by 1 byte for alignment                      │
│  - 56 bits consumed per refill cycle                                 │
│  - Efficient use of memory bandwidth                                 │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Byte Alignment

Before reading Huffman table headers, the bitstream is aligned to a byte boundary:

```c
// Calculate bits to skip for byte alignment
uint64_t alignment_bits = (0 - bits_remaining) & 0x07;
discard_bits(&bit_buffer, &stream_ptr, &bits_remaining, alignment_bits);
```

---

## Decompression Process

### Overall Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Decompression Flow                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. Read Header                                                      │
│     ├── Verify magic "br" (0x7262)                                   │
│     └── Read file count                                              │
│                                                                      │
│  2. For each file:                                                   │
│     ├── Read file type (must be 0)                                   │
│     ├── Read decompressed size                                       │
│     ├── Read destination address (KSEG0)                             │
│     │                                                                │
│     ├── Initialize MTE tables                                        │
│     │   └── Both tables: [31, 30, 29, ..., 1, 0]                     │
│     │                                                                │
│     ├── Load 24 Huffman table pointers                               │
│     │   ├── Byte-align bitstream                                     │
│     │   ├── Read 16-bit table size                                   │
│     │   ├── Store current stream position as table base              │
│     │   └── Seek past table data                                     │
│     │                                                                │
│     └── Decode instructions loop:                                    │
│         ├── Decode opcode (Huffman table 0)                          │
│         ├── Look up format_flags from opcode table                   │
│         ├── Handle special opcodes:                                  │
│         │   ├── opcode=0: SPECIAL (decode funct, use funct_table)    │
│         │   ├── opcode=1: REGIMM (decode rt, use regimm_table)       │
│         │   └── 0xFFFF: Extended (decode rs, use extended_table)     │
│         ├── Decode fields based on format_flags                      │
│         ├── Assemble 32-bit instruction                              │
│         └── Write to destination, advance pointer                    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Instruction Assembly

After decoding all fields, the instruction is assembled:

```c
// R-type assembly
instruction = (opcode << 26) | (rs << 21) | (rt << 16) |
              (rd << 11) | (shamt << 6) | funct;

// I-type assembly
instruction = (opcode << 26) | (rs << 21) | (rt << 16) | immediate;

// J-type assembly
instruction = (opcode << 26) | target;
```

---

## Compression Process

### Two-Pass Algorithm

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Compression Flow                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Pass 1: Frequency Collection                                        │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │ For each instruction:                                       │     │
│  │   1. Decode into fields (opcode, rs, rt, rd, shamt, etc.)   │     │
│  │   2. Determine format_flags from lookup tables              │     │
│  │   3. For each field to encode:                              │     │
│  │      - If MTE: simulate MTE to get index, count index       │     │
│  │      - If direct: count raw value                           │     │
│  │   4. Update frequency counters for appropriate tables       │     │
│  │                                                             │     │
│  │ After all instructions:                                     │     │
│  │   Build Huffman codes from frequencies                      │     │
│  │   Serialize Huffman trees to skip-list format               │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                                                                      │
│  Pass 2: Encoding                                                    │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │ Write header (magic, file count)                            │     │
│  │                                                             │     │
│  │ For each file:                                              │     │
│  │   1. Write file header (type, size, address)                │     │
│  │   2. Write 24 Huffman tables (size + data, byte-aligned)    │     │
│  │   3. Reset MTE tables                                       │     │
│  │   4. For each instruction:                                  │     │
│  │      - Encode opcode                                        │     │
│  │      - Encode additional fields based on format_flags       │     │
│  │      - Update MTE tables as we go                           │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Critical Implementation Details

1. **MTE State Synchronization**: The MTE tables must be updated identically during both frequency collection and encoding passes, and must match the decompressor's behavior exactly.

2. **Format Flags Lookup Order**: The field encoding order must follow the decompressor's decoding order:
   - rt first (if applicable)
   - rs second (if applicable)
   - rd third (if applicable)
   - shamt fourth (if applicable)
   - jump target or immediate last

3. **Special Case Handling**:
   - JR $ra (0x03E00008): No special case — it uses `funct_table[8]` = 0x00A9
     like any other JR, so its `rs` is MTF1-encoded and must be counted in the
     frequency pass as well as emitted in the encoding pass
   - NOP (0x00000000): Still needs field encoding (SLL $zero, $zero, 0)
   - COP branch with rt<4: Uses immediate type 3 tables

---

## Lookup Tables

### Opcode Table (64 entries)

Maps MIPS opcodes (0-63) to format flags:

```
Opcode │ Mnemonic │ Flags  │ Field Encoding
───────┼──────────┼────────┼─────────────────────────────────────
   0   │ SPECIAL  │ 0x0000 │ → Uses funct_table
   1   │ REGIMM   │ 0x0000 │ → Uses regimm_table
   2   │ J        │ 0x0100 │ 26-bit target (split)
   3   │ JAL      │ 0x0100 │ 26-bit target (split)
   4   │ BEQ      │ 0x0C05 │ rs:MTF1, rt:MTF1, imm:type3
   5   │ BNE      │ 0x0C05 │ rs:MTF1, rt:MTF1, imm:type3
   8   │ ADDI     │ 0x0805 │ rs:MTF1, rt:MTF1, imm:type2
   9   │ ADDIU    │ 0x0805 │ rs:MTF1, rt:MTF1, imm:type2
  15   │ LUI      │ 0x0806 │ rs:direct, rt:MTF1, imm:type2
16-18  │ COPx     │ 0xFFFF │ → Uses extended_table
  35   │ LW       │ 0x0605 │ rs:MTF2, rt:MTF1, imm:type1
  43   │ SW       │ 0x0605 │ rs:MTF2, rt:MTF1, imm:type1
```

### Function Table (64 entries, for opcode=0)

Maps SPECIAL function codes to format flags:

```
Funct │ Mnemonic │ Flags  │ Field Encoding
──────┼──────────┼────────┼─────────────────────────────────────
   0  │ SLL      │ 0x0056 │ rt:MTF1, rd:MTF1, shamt:huff
   2  │ SRL      │ 0x0056 │ rt:MTF1, rd:MTF1, shamt:huff
   4  │ SLLV     │ 0x0095 │ rs:MTF1, rt:MTF1, rd:MTF1
   8  │ JR       │ 0x00A9 │ rs:MTF1 (including JR $ra)
   9  │ JALR     │ 0x0099 │ rs:MTF1, rd:MTF1
  16  │ MFHI     │ 0x009A │ rd:MTF1
  24  │ MULT     │ 0x00A5 │ rs:MTF1, rt:MTF1
  32  │ ADD      │ 0x0095 │ rs:MTF1, rt:MTF1, rd:MTF1
  33  │ ADDU     │ 0x0095 │ rs:MTF1, rt:MTF1, rd:MTF1
```

### REGIMM Table (32 entries, for opcode=1)

Maps REGIMM rt field to format flags:

```
RT   │ Mnemonic │ Flags  │ Field Encoding
─────┼──────────┼────────┼─────────────────────────────────────
  0  │ BLTZ     │ 0x0C01 │ rs:MTF1, imm:type3
  1  │ BGEZ     │ 0x0C01 │ rs:MTF1, imm:type3
  8  │ TGEI     │ 0x0801 │ rs:MTF1, imm:type2
 16  │ BLTZAL   │ 0x0C01 │ rs:MTF1, imm:type3
 17  │ BGEZAL   │ 0x0C01 │ rs:MTF1, imm:type3
```

### Extended Table (32 entries, for COP0/COP1/COP2 rs field)

```
RS   │ Operation │ Flags  │ Field Encoding
─────┼───────────┼────────┼─────────────────────────────────────
 0-2 │ MFC/CFC   │ 0x1094 │ rt:MTF1, rd:direct
 4-6 │ MTC/CTC   │ 0x1094 │ rt:MTF1, rd:direct
  8  │ BC        │ 0x0C00 │ imm:type3 (coprocessor branch)
16-31│ CO        │ 0xFFFF │ Raw 21-bit value
```

---

## Usage

Build with `make -C tools/kipack`. The result is a single `kipack` binary with
two subcommands.

### Decompression (unpack)

```bash
# Decompress a KI1 ROM
kipack unpack roms/ki-l15d.u98 output_dir/

# Decompress a KI2 ROM
kipack unpack roms/ki2-l14k.u98 output_dir/

# Decompress a packed file
kipack unpack repacked.packed output_dir/

# Name does not identify the variant? Say so explicitly.
kipack unpack -t ki2 dump.bin output_dir/
```

The variant is guessed from the file name — `ki2-`/`ki2_` for KI2, `ki-p47` for
the prototype, `packed`/`.packed` for pack's own output, KI1 otherwise. `-t`
overrides the guess and accepts `ki1`, `ki1-p47`, `ki2` or `packed`.

The output directory must already exist.

Output files:
- `rom-0.bin` - first decompressed code segment
- `rom-0.addr` - 4-byte file holding the load address
- `rom-1.bin`, `rom-1.addr`, and so on for the remaining segments

### Compression (pack)

```bash
# Compress a directory of segments
kipack pack input_dir/ output.packed

# Cap how many segments are considered
kipack pack output_dir/ repacked.packed 2
```

Input files must be named `rom-N.bin` and `rom-N.addr`, starting at N=0. pack
stops at the first missing pair.

### Embedding a subcommand in another tool

Both applets are built to be linked elsewhere. Each exports a typed entry point
and an argv shim, and nothing else:

```c
int kipack_unpack(const char *rom_path, const char *out_dir, kipack_variant_t variant);
int kipack_unpack_main(int argc, char **argv);

int kipack_pack(const char *in_dir, const char *out_path, int max_files);
int kipack_pack_main(int argc, char **argv);
```

Compile the applet's `.c` file into your tool and either call the typed function
directly or add a row to your own dispatch table.

---

## Testing

```bash
make -C tools/kipack check
```

30 checks, in four groups:

| Group | Checks | Needs ROMs |
|-------|--------|------------|
| Synthetic | 14 | no |
| Baseline manifests | 2 | yes |
| Round-trip per ROM | 13 | yes |
| Size sweep | 1 | yes |

The **synthetic** group needs no ROM data at all, so it runs on any clone. It
builds a corpus of MIPS words from `fixtures/mkcorpus.c` — one instruction per
branch of the format-flag dispatch, plus 256 `addiu` words that push a Huffman
subtree past 128 bytes and so exercise the 2-byte skip count every real ROM
uses — packs it, and unpacks it back. `fixtures/mkromimage.c` then rewrites that
same stream at each real variant's table offsets, which lets the KI1, KI1-p47
and KI2 table layouts and all four arms of the file-name heuristic be tested
without shipping a single byte of Rare's data. `fixtures/mkbadstream.c` builds
deliberately malformed `.packed` archives that reach the bounds guards a
single-byte poke cannot: the ROM window is 512 KB and zero-filled, so running
a short stream off the end takes a chain of table offsets, each landing
exactly where the next is read from, rather than one flipped byte.

Six of the fourteen are negative checks: they hand `unpack` an archive with a
bad magic, a zero file count, a size that is not a multiple of 4, a table
offset that seeks past the ROM window, or a Huffman tree that walks past the
ROM window, and hand `pack` a segment that is not whole instructions. The
zero file count and the non-multiple-of-4 size used to hang the decoder
forever, so every negative check runs under a wall-clock bound regardless and
must exit with the applet's own code 1 — a regression to hanging reports the
kill signal instead, and fails.

The remaining groups read `assets/roms/*.u98`, which are not in the repository.
With no ROMs present the suite reports **14 passed** and exits 0 rather than
failing; `ROMS=<dir>` points it at another ROM directory.

The **baseline manifests** in `fixtures/expected/` are SHA-256 lists of every
unpacked segment and every repacked archive. They are the guard against silent
output changes: any edit that alters a compressed byte shows up here.

```bash
./check.sh --update    # regenerate the manifests
```

Only do that when the output was *meant* to change, and say why in the commit
message. `--update` refuses to run while any of the 14 synthetic checks is
failing — otherwise a broken codec could overwrite the manifests with its own
output and report success — and refuses to run with no ROMs present, rather
than exiting 0 having written nothing.

The suite no longer depends on any untracked tool. It used to include a parity
group that cross-checked `unpack` output against a separate, untracked
decompressor; that group was retired once the older tool was dropped from the
build, leaving the baseline manifests and the synthetic corpus as the sole
oracles — so nothing in the suite needs anything a fresh clone does not have.

---

## Technical Notes

### Why Different Compressed Data?

When round-tripping (decompress → recompress), the compressed output may differ from the original while still decompressing to identical data. This is because:

1. **Huffman Trees**: The tree structure depends on symbol frequencies and tie-breaking during construction. Different tree structures produce different but equivalent codes.

2. **Tree Serialization**: The skip-list format can represent the same tree differently depending on which branch is considered "left" vs "right".

3. **No Canonicalization**: The original compressor doesn't canonicalize Huffman codes (e.g., by code length), so equivalent but different trees are valid.

### Memory Addresses

The KI bootrom uses MIPS virtual addressing:
- **KSEG0** (0x80000000-0x9FFFFFFF): Cached, unmapped
- **KSEG1** (0xA0000000-0xBFFFFFFF): Uncached, unmapped

Decompressed code is loaded to KSEG0 addresses (e.g., 0x80010000).

### Endianness

- **Tables**: Little-endian 16-bit values
- **Instructions**: Little-endian 32-bit values
- **Bitstream**: LSB-first bit extraction

---

## References

- MIPS R4000 User's Manual
- Original Killer Instinct arcade game ROMs (Rare/Midway, 1994-1996)
