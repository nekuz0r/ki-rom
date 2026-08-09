#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kipack.h"
#include "unpack.h"

/*
 * Write a whole file, or fail loudly.
 *
 * Unchecked, a short write here produces a truncated segment that `make rom`
 * then compresses and links into the ROM as if it were complete.
 */
static int write_file(const char *path, const void *data, size_t length)
{
	FILE *fd = fopen(path, "wb");
	if (fd == NULL)
	{
		fprintf(stderr, "unpack: %s: cannot create\n", path);
		return -1;
	}
	if (length != 0 && fwrite(data, 1, length, fd) != length)
	{
		fprintf(stderr, "unpack: %s: short write\n", path);
		fclose(fd);
		return -1;
	}
	// Buffered data is flushed here, so a full disk can surface at close.
	if (fclose(fd) != 0)
	{
		fprintf(stderr, "unpack: %s: write failed on close\n", path);
		return -1;
	}
	return 0;
}

/*
 * Explicit little-endian 32-bit store.
 *
 * The decoded stream is little-endian MIPS whatever the host is, and rom-N.bin
 * has to come out byte-identical on any machine. Written out byte by byte
 * rather than through a cast, which would be an unaligned store and would
 * inherit the host's byte order. GCC at -O3 folds this back into a single
 * store on x86-64 and arm64.
 */
static void store_le32(uint8_t *p, uint32_t value)
{
	p[0] = (uint8_t)(value);
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

static int dump_bin(const char *output, int id, const uint8_t *data,
                    uint32_t length, uint32_t load_addr)
{
	char filename[256] = {0};
	char addr_filename[256] = {0};
	snprintf(filename, sizeof(filename), "%s/rom-%d.bin", output, id);
	snprintf(addr_filename, sizeof(addr_filename), "%s/rom-%d.addr", output, id);

	if (write_file(filename, data, length) != 0)
	{
		return -1;
	}

	// Raw host-endian 4 bytes. pack.c reads it back the same way and ORs
	// 0x80000000 to rebuild the virtual address, so this encoding is a
	// contract between the two applets rather than a choice made here.
	if (write_file(addr_filename, &load_addr, sizeof(load_addr)) != 0)
	{
		return -1;
	}

	printf("unpacked rom-%d @ 0x%08lx (length = 0x%x)\n",
	       id, (unsigned long)load_addr, length);
	return 0;
}

/*
 * The ROM window.
 *
 * Everything the decoder reads lives here: the compressed stream, the Huffman
 * trees, and the four format lookup tables. The eight trailing bytes are
 * padding for the reader's 64-bit prefetch - a cursor sitting on the window's
 * last byte still loads eight bytes, and used to get them from the 512 MB
 * array that stood in for the address space. Keeping the padding zeroed means
 * that read returns exactly what it always did.
 */
#define ROM_WINDOW_SIZE 0x80000
#define ROM_LOOKAHEAD   8

static uint8_t rom_image[ROM_WINDOW_SIZE + ROM_LOOKAHEAD];

/* First byte past the window proper. The padding is not addressable data. */
#define ROM_END (rom_image + ROM_WINDOW_SIZE)

/*
 * Explicit little-endian loads.
 *
 * The stream is little-endian whatever the host is, and reading it through a
 * cast would be an unaligned access as well as an inherited byte order. GCC at
 * -O3 folds each of these back into a single load on x86-64 and arm64.
 */
static uint16_t load_le16(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t load_le64(const uint8_t *p)
{
	uint64_t value = 0;

	for (int i = 7; i >= 0; i--)
	{
		value = (value << 8) | p[i];
	}
	return value;
}

static int load_rom(const char *filename)
{
	FILE *fd = fopen(filename, "rb");
	if (fd == NULL)
	{
		fprintf(stderr, "unpack: %s: cannot open\n", filename);
		return -1;
	}

	// Clear the window first. A .packed file is far shorter than a boot ROM,
	// so a short read is normal here and must not be treated as an error - but
	// without the clear, a second call in the same process would decode over
	// whatever the previous ROM left behind. Cleared by sizeof rather than
	// ROM_WINDOW_SIZE so the prefetch padding is zeroed too: a lookahead at
	// the very end of the window reads it.
	memset(rom_image, 0, sizeof(rom_image));

	size_t got = fread(rom_image, 1, ROM_WINDOW_SIZE, fd);
	int failed = ferror(fd);
	fclose(fd);

	if (failed)
	{
		fprintf(stderr, "unpack: %s: read error\n", filename);
		return -1;
	}
	if (got == 0)
	{
		fprintf(stderr, "unpack: %s: empty\n", filename);
		return -1;
	}
	return 0;
}

/**
 * Bitstream reader.
 *
 * Replaces the (bit_buffer, stream_ptr, bits_remaining) triple that every
 * reader helper used to thread through its argument list. The boot ROM kept
 * this state in a memory structure because it had to survive across calls;
 * here it is a local, so init_bitstream_state() and load_bitstream_state() -
 * which wrote it to DRAM and read it straight back on the next line - are gone
 * with it.
 */
typedef struct
{
	uint64_t       bits;    // 64-bit buffer holding prefetched bits
	const uint8_t *cursor;  // Position in the stream (points 7 bytes ahead)
	int8_t         count;   // Bits left in the buffer (negative, -56 when full)
	int            error;   // Sticky: set once a read leaves the ROM window
} bitreader_t;

/*
 * Fetch the reader's 64-bit lookahead, or fail.
 *
 * The old pram() masked every address to 29 bits, so a read could never be out
 * of range - a corrupt stream quietly got bytes from elsewhere in a 512 MB
 * array and decoded nonsense. With a real buffer the range is real, so
 * out-of-range has to be an answer. Returning zero keeps every caller's
 * arithmetic in bounds until the decode loop notices the flag.
 */
static uint64_t bitreader_load(bitreader_t *br)
{
	if (br->cursor < rom_image || br->cursor + 8 > rom_image + sizeof(rom_image))
	{
		if (!br->error)
		{
			fprintf(stderr, "unpack: bitstream ran past the ROM window\n");
		}
		br->error = 1;
		return 0;
	}
	return load_le64(br->cursor);
}

/**
 * Points the reader at compressed data and fills its buffer.
 *
 * @param br      Reader to initialise
 * @param source  Start of the compressed data to read from
 */
static void bitreader_init(bitreader_t *br, const uint8_t *source)
{
	br->cursor = source;
	br->count = -56;  // -0x38
	br->error = 0;
	br->bits = bitreader_load(br);
	br->cursor = source + 0x7;
}

/**
 * Reads a specified number of bits from the bitstream.
 *
 * @param br        Reader state
 * @param num_bits  Number of bits to extract (1-32)
 * @return          The extracted bits as a uint64_t value
 */
static uint64_t read_bits(bitreader_t *br, uint8_t num_bits)
{
	// Read next 64 bits from stream (lookahead)
	uint64_t next_bits = bitreader_load(br);

	// Convert negative remaining count to positive shift amount
	int8_t shift_amount = 0 - br->count;

	// Shift lookahead bits and merge into buffer
	uint64_t shifted_bits = next_bits << shift_amount;
	br->bits = br->bits | shifted_bits;

	// Calculate how many bits we'll be short after this read
	br->count = num_bits - shift_amount;

	// Create bitmask for extracting num_bits (e.g., num_bits=8 -> mask=0xFF)
	uint64_t bitmask = (1ULL << num_bits) - 1;

	// Extract the requested bits from buffer
	uint64_t result = br->bits & bitmask;

	// Check if we need to refill the buffer
	if (br->count <= 0)
	{
		// We have enough bits - just shift out the consumed bits
		br->bits = br->bits >> num_bits;
		return result;
	}

	// Need to refill: advance stream pointer by 7 bytes
	br->cursor = br->cursor + 0x7;

	// Reload buffer from the lookahead, shifted to align remaining bits
	br->bits = next_bits >> br->count;

	// Reset remaining count: we now have 56 bits minus what we still need
	br->count = br->count - 56;  // -0x38

	return result;
}

static uint64_t read_16_bits(bitreader_t *br)
{
	return read_bits(br, 16);
}

static uint64_t read_8_bits(bitreader_t *br)
{
	return read_bits(br, 8);
}

static uint64_t read_32_bits(bitreader_t *br)
{
	return read_bits(br, 32);
}

/*
 * Workspace slot indices.
 *
 * The boot ROM kept twenty-four 4-byte Huffman table pointers at offsets
 * 0x40..0x9C of a fixed DRAM workspace; index = (offset - 0x40) / 4. The old
 * offset is given against each name so the numbering stays checkable against
 * the disassembly.
 */
enum
{
	TBL_OPCODE     =  0,  /* 0x40 - opcode                          */
	TBL_FUNCT      =  1,  /* 0x44 - funct, for SPECIAL              */
	TBL_REGIMM     =  2,  /* 0x48 - REGIMM rt                       */
	TBL_RS_MTE     =  3,  /* 0x4C - rs via MTE table 1              */
	TBL_RT_MTE     =  4,  /* 0x50 - rt via MTE table 1              */
	TBL_RD_MTE     =  5,  /* 0x54 - rd via MTE table 1              */
	TBL_SHAMT      =  6,  /* 0x58 - shamt                           */
	TBL_RS_MTE2    =  7,  /* 0x5C - rs via MTE table 2              */
	TBL_DIRECT     =  8,  /* 0x60 - any field decoded without MTE   */
	TBL_BR_IMM_HI  =  9,  /* 0x64 - branch immediate, high byte     */
	TBL_BR_IMM_LO  = 10,  /* 0x68 - branch immediate, low byte      */
	TBL_TARGET_0   = 11,  /* 0x6C - jump target, bits 20+           */
	TBL_TARGET_1   = 12,  /* 0x70 - jump target, bits 14+           */
	TBL_TARGET_2   = 13,  /* 0x74 - jump target, bits 7+            */
	TBL_TARGET_3   = 14,  /* 0x78 - jump target, low bits           */
	TBL_IMM1_HI    = 15,  /* 0x7C - immediate type 1, high byte     */
	TBL_IMM1_LO    = 16,  /* 0x80 - immediate type 1, low byte      */
	TBL_IMM2_HI    = 17,  /* 0x84 - immediate type 2, high byte     */
	TBL_IMM2_LO    = 18,  /* 0x88 - immediate type 2, low byte      */
	TBL_IMM3_HI    = 19,  /* 0x8C - immediate type 3, high byte     */
	TBL_IMM3_LO    = 20,  /* 0x90 - immediate type 3, low byte      */
	TBL_EXT_REG    = 21,  /* 0x94 - REGIMM rs, and extended-format rt */
	TBL_EXT_CODE   = 22,  /* 0x98 - extended format code            */
	TBL_COP_RT     = 23,  /* 0x9C - coprocessor rt                  */
};

/*
 * Decompression workspace.
 *
 * The boot ROM parked this 160-byte block at a fixed DRAM address because it
 * had nowhere else to put it. Here it is a local of decompress_rom().
 */
typedef struct
{
	uint8_t        mtf[2][32];                     /* was workspace 0x00 and 0x20 */
	const uint8_t *tables[KIPACK_NUM_TABLES];      /* was workspace 0x40 .. 0x9C  */
} workspace_t;

/**
 * Initializes two 32-byte Move-to-End (MTE) tables with descending values.
 *
 * Each table is filled with values [31, 30, 29, ..., 1, 0] at indices [0, 1, 2, ..., 30, 31].
 * Table 1 is at offset 0x00, Table 2 is at offset 0x20 (32 bytes apart).
 *
 * These tables are used by the MTE transform during decompression, where recently
 * used symbols are moved to the end of the list for better compression.
 *
 * @param ws  Workspace holding the two 32-byte MTE tables
 */
static void init_mtf_tables(workspace_t *ws)
{
	// Both tables start as [31, 30, ..., 1, 0]
	for (int i = 0; i < 32; i++)
	{
		ws->mtf[0][i] = (uint8_t)(31 - i);
		ws->mtf[1][i] = (uint8_t)(31 - i);
	}
}

/**
 * Discards a number of bits without returning them, for byte alignment.
 *
 * @param br            Reader state
 * @param bits_to_skip  Number of bits to discard (typically 0-7)
 */
static void discard_bits(bitreader_t *br, uint64_t bits_to_skip)
{
	// Consume the specified bits from the remaining count
	br->count = br->count + bits_to_skip;

	// Shift out the discarded bits from the buffer
	br->bits = br->bits >> bits_to_skip;

	// Check if we still have bits in the buffer
	if (br->count <= 0)
	{
		return;  // Still have bits available, done
	}

	// Need to refill the buffer
	br->bits = bitreader_load(br);
	br->cursor = br->cursor + 0x7;
	br->bits = br->bits >> br->count;
	br->count = br->count - 56;  // -0x38
}

/**
 * Calculates the byte-aligned position the reader is currently at.
 *
 * Since count is negative (e.g. -56 when full), negating it gives the number
 * of bits still buffered, and dividing by 8 gives how many bytes the cursor is
 * ahead of the real position.
 *
 * @param br  Reader state
 * @return    The byte-aligned base address
 */
static const uint8_t *get_stream_byte_address(const bitreader_t *br)
{
	// Convert negative count to positive bits available. The cast keeps the
	// truncation the old uint8_t parameter performed.
	uint64_t bits_available = (0 - (uint8_t)br->count) & 0xFF;

	// Bits to bytes, then back off the stream pointer to the base address
	return br->cursor - (bits_available >> 3);
}

/**
 * Seeks the bitstream reader to a specific byte address.
 *
 * @param br            Reader state
 * @param byte_address  The byte address to seek to
 */
static void seek_bitstream(bitreader_t *br, const uint8_t *byte_address)
{
	br->cursor = byte_address;

	// Load 64 bits from the target address into the bit buffer
	br->bits = bitreader_load(br);

	// Buffer is full with 56 usable bits
	br->count = -56;  // -0x38

	// Set stream pointer to 7 bytes ahead (standard lookahead offset)
	br->cursor = byte_address + 0x7;
}

/**
 * Decodes a Huffman symbol by navigating a skip-list encoded binary tree.
 *
 * The tree is stored as a linear array where each node contains a skip count:
 *   - Positive byte (1-127): skip count for left branch
 *   - Zero byte: terminal node (leaf) - return current position
 *   - Negative byte: extended 15-bit skip count (high 7 bits + next byte)
 *
 * Navigation uses one bit from the bitstream per node:
 *   - Bit = 1: take right branch (continue to next sequential entry)
 *   - Bit = 0: take left branch (skip forward by the count)
 *
 * @param table_ptr  Starting address in the Huffman tree table
 * @param br         Reader state
 * @return           Address of the decoded symbol in the table
 */
static const uint8_t *decode_huffman_symbol(const uint8_t *table_ptr, bitreader_t *br)
{
	uint64_t decision_bit;
	do
	{
		// The skip counts come off the compressed stream, so a corrupt tree
		// can walk anywhere. The 512 MB array absorbed that silently.
		if (table_ptr < rom_image || table_ptr >= ROM_END)
		{
			if (!br->error)
			{
				fprintf(stderr, "unpack: Huffman tree walked past the ROM window\n");
			}
			br->error = 1;
			return rom_image;
		}

		// Read skip count from current node (signed byte)
		int16_t skip_count = (int8_t)*table_ptr;
		table_ptr = table_ptr + 1;

		// Get next bit from bitstream for branch decision
		decision_bit = br->bits & 0x1;

		// Handle skip count encoding
		if (skip_count <= 0)
		{
			// Zero means terminal node - we found our symbol
			if (skip_count == 0)
			{
				return table_ptr;
			}

			// Negative: extended 15-bit skip count
			// Format: [1][7-bit high] [8-bit low]
			if (table_ptr < rom_image || table_ptr >= ROM_END)
			{
				if (!br->error)
				{
					fprintf(stderr, "unpack: Huffman tree walked past the ROM window\n");
				}
				br->error = 1;
				return rom_image;
			}

			skip_count = skip_count & 0x7f;              // Extract high 7 bits
			skip_count = skip_count << 8;                // Shift to high byte
			uint8_t low_byte = *table_ptr;               // Read low 8 bits
			skip_count = skip_count + low_byte;          // Combine into 15-bit value
			table_ptr = table_ptr + 1;
		}

		// Consume one bit from the bitstream
		br->count = br->count + 1;
		br->bits = br->bits >> 1;

		// Refill bit buffer if exhausted
		if (br->count > 0)
		{
			br->bits = bitreader_load(br);
			br->cursor = br->cursor + 0x7;
			br->bits = br->bits >> br->count;
			br->count = br->count - 56;
		}

		if (br->error)
		{
			return rom_image;
		}

		// Branch decision based on the bit:
		// bit=1: go right (next entry), bit=0: go left (skip forward)
		if (decision_bit == 0)
		{
			table_ptr = table_ptr + skip_count;
		}

	} while (1);
}

/**
 * Move-to-End (MTE) update operation on a 32-byte table.
 *
 * Finds the given symbol in the table and moves it to the END - index 31 -
 * shifting everything after it down by one position.
 *
 * Example: If table is [A, B, C, D, E] and symbol is 'B':
 *   1. Find 'B' at index 1
 *   2. Shift elements down: [A, C, D, E, E]
 *   3. Insert 'B' at the end: [A, C, D, E, B]
 *
 * This is NOT the Move-to-Front of the compression literature, where recently
 * used symbols move to index 0. The KI algorithm moves them to index 31, so
 * recently used registers get the HIGH indices and the Huffman tables for
 * those indices are weighted accordingly. See "Why Move-to-End?" in README.md,
 * and mtf_encode() in pack.c, which is the matching encoder.
 *
 * Returns 0 on success, -1 when the symbol is not one of the table's 32
 * entries.
 *
 * @param table   The 32-byte MTE table
 * @param symbol  The symbol value to find and move to the end
 */
static int mtf_move_to_end(uint8_t *table, uint8_t symbol)
{
	int i;

	// PHASE 1: Search for the symbol in the table.
	//
	// Bounded at the table's 32 entries. `symbol` is read out of the workspace
	// at an index the bitstream decoded, so a corrupt stream can ask for a
	// value that is in no table at all. Unbounded, the scan then ran off the
	// end of the 32-byte table and through the rest of the workspace until
	// some byte happened to match, or spun forever.
	for (i = 0; i < 32; i++)
	{
		if (table[i] == symbol)
		{
			break;  // Found the symbol
		}
	}
	if (i == 32)
	{
		fprintf(stderr, "unpack: MTE symbol 0x%02x is not in the table\n", symbol);
		return -1;
	}

	// PHASE 2: Shift the elements after it down one slot
	for (; i < 31; i++)
	{
		table[i] = table[i + 1];
	}

	// PHASE 3: Place the symbol at its new position (index 31, the end)
	table[31] = symbol;
	return 0;
}

/**
 * Reads the register held at `index` in a 32-entry MTE table, moves it to the
 * end of that table, and returns it.
 *
 * `index` is a symbol the Huffman decoder produced, so it is a full byte and a
 * corrupt table can put it past the 32 entries. Reject that rather than
 * reading - and then reordering around - whatever follows the table in the
 * workspace.
 *
 * @param table  The 32-byte MTE table
 * @param index  Table index decoded from the bitstream
 * @return       The register value, or -1 if the index is out of range
 */
static int64_t mtf_decode_reg(uint8_t *table, uint64_t index)
{
	if (index >= 32)
	{
		fprintf(stderr, "unpack: MTE index %lu is past the 32 table entries\n",
		        (unsigned long)index);
		return -1;
	}

	uint8_t value = table[index];
	if (mtf_move_to_end(table, value) != 0)
	{
		return -1;
	}
	return (int64_t)value;
}

/**
 * ROM offsets for decompression lookup tables.
 *
 * These offsets (added to ROM base 0x1fc00000) point to critical data
 * structures used during MIPS code decompression. Each ROM version has
 * slightly different offsets due to bootrom code changes.
 */
typedef struct
{
	uint16_t compressed_data;    // Start of compressed code stream
	uint16_t opcode_table;       // Opcode (0-63) -> format flags lookup table
	uint16_t funct_table;        // Function code lookup (for SPECIAL, opcode=0)
	uint16_t regimm_table;       // REGIMM rt field lookup (for opcode=1)
	uint16_t extended_table;     // Extended format lookup (when flags=0xFFFF)
} rom_offsets_t;

/**
 * ROM offset tables for each Killer Instinct version.
 *
 * ROM Layout (example for KI1):
 *   0x1fc00e48  Function code table (128 bytes = 64 entries × 2 bytes)
 *   0x1fc00ec8  REGIMM table (64 bytes = 32 entries × 2 bytes)
 *   0x1fc00f08  Extended format table
 *   0x1fc00f48  Opcode table (128 bytes = 64 entries × 2 bytes)
 *   0x1fc00fd0  Compressed data stream start
 */
static const rom_offsets_t rom_offsets[] = {
	// KI1 (Killer Instinct 1, original)
	{
		.compressed_data = 0x0fd0,
		.opcode_table    = 0x0f48,
		.funct_table     = 0x0e48,
		.regimm_table    = 0x0ec8,
		.extended_table  = 0x0f08,
	},
	// KI1 p47 (Killer Instinct 1, prototype 47)
	{
		.compressed_data = 0x1010,
		.opcode_table    = 0x0f80,
		.funct_table     = 0x0e80,
		.regimm_table    = 0x0f00,
		.extended_table  = 0x0f40,
	},
	// KI2 (Killer Instinct 2)
	{
		.compressed_data = 0x1040,
		.opcode_table    = 0x0fb0,
		.funct_table     = 0x0eb0,
		.regimm_table    = 0x0f30,
		.extended_table  = 0x0f70,
	},
	// PACKED (output from the pack applet)
	{
		.compressed_data = KIPACK_PACKED_DATA,
		.opcode_table    = KIPACK_PACKED_OPCODE,
		.funct_table     = KIPACK_PACKED_FUNCT,
		.regimm_table    = KIPACK_PACKED_REGIMM,
		.extended_table  = KIPACK_PACKED_EXTENDED,
	},
};

#define ROM_OFFSETS_COUNT (sizeof(rom_offsets) / sizeof(rom_offsets[0]))

// Currently selected ROM offsets
static const rom_offsets_t *current_rom = &rom_offsets[KIPACK_KI1];

/**
 * Decompresses MIPS R4000 executable code from Killer Instinct arcade ROM.
 *
 * This function implements a custom decompression algorithm that:
 * 1. Reads compressed data from ROM
 * 2. Decodes multiple embedded files (each containing MIPS code)
 * 3. Reconstructs 32-bit MIPS instructions from compressed components
 * 4. Uses Huffman coding + Move-to-End transform for compression
 *
 * MIPS instruction format (32 bits):
 *   [opcode:6][rs:5][rt:5][rd:5][shamt:5][funct:6]  (R-type)
 *   [opcode:6][rs:5][rt:5][immediate:16]           (I-type)
 *   [opcode:6][target:26]                          (J-type)
 *
 * @param output_dir  Directory path to write decompressed files
 */
static int decompress_rom(const char *output_dir)
{
	const uint8_t *compressed_data = rom_image + current_rom->compressed_data;
	workspace_t ws;
	bitreader_t br;
	uint64_t entry_point = 0;  // Will hold address of first decompressed file (jump target)

	bitreader_init(&br, compressed_data);

	// Verify magic number "br" (0x7262 = 'b' 'r' for "binary ROM")
	uint64_t magic = read_16_bits(&br);
	if (magic != KIPACK_MAGIC)
	{
		fprintf(stderr, "unpack: invalid magic: expected 0x%x, got 0x%lx\n",
		        KIPACK_MAGIC, (unsigned long)magic);
		return -1;
	}

	// Read number of compressed files
	uint64_t file_count = read_8_bits(&br);

	// The file loop below is a do/while that decrements after each pass, so a
	// count of zero underflows to UINT64_MAX and spins. pack never emits it -
	// it refuses a run with no input pairs - so any stream carrying it is
	// corrupt.
	if (file_count == 0)
	{
		fprintf(stderr, "unpack: file count is zero\n");
		return -1;
	}

	uint64_t total_files = file_count;
	uint64_t files_remaining = file_count;

	// Process each compressed file
	do
	{
		// Read file type (must be 0 for code files)
		uint64_t file_type = read_8_bits(&br);
		if (file_type != 0)
		{
			fprintf(stderr, "unpack: unsupported file type %lu\n",
			        (unsigned long)file_type);
			return -1;
		}

		// Read decompressed size and destination address
		uint64_t decompressed_size = read_32_bits(&br);

		// The instruction loop below counts this down by 4 and stops at exactly
		// 0, so a size that is not a whole number of 32-bit instructions wraps
		// its unsigned counter and spins. pack refuses such an input at load
		// time; refuse it here too, because unpack is the side that reads
		// streams it did not produce.
		if (decompressed_size % 4 != 0)
		{
			fprintf(stderr, "unpack: file %lu: size %lu is not a multiple of 4\n",
			        (unsigned long)(total_files - files_remaining),
			        (unsigned long)decompressed_size);
			return -1;
		}

		uint64_t original_size = decompressed_size;

		uint64_t dest_addr = read_32_bits(&br);
		dest_addr = (dest_addr << 32) >> 32;  // Sign-extend to 64 bits

		// Remember entry point (first file's destination = boot address)
		if (entry_point == 0)
		{
			entry_point = dest_addr;
		}

		// The decoded bytes go into a right-sized buffer of their own rather
		// than into a shared fake DRAM at dest_addr, so the write cannot leave
		// the file it belongs to. dest_addr survives only as the number
		// recorded in rom-N.addr.
		//
		// This is the formula the instruction writes actually used, via the
		// old pram() masking to 29 bits. It is the exact inverse of pack.c's
		// `load_addr | 0x80000000`.
		uint32_t load_addr = (uint32_t)dest_addr & 0x1FFFFFFF;
		uint32_t written = 0;
		uint8_t *out = NULL;

		if (original_size != 0)
		{
			out = malloc((size_t)original_size);
			if (out == NULL)
			{
				fprintf(stderr, "unpack: file %lu: cannot allocate %lu bytes\n",
				        (unsigned long)(total_files - files_remaining),
				        (unsigned long)original_size);
				return -1;
			}
		}

		if (decompressed_size != 0)
		{
			// Initialize Move-to-End tables for register encoding
			init_mtf_tables(&ws);

			// Load the 24 Huffman table pointers
			for (int i = 0; i < KIPACK_NUM_TABLES; i++)
			{
				// Byte-align the bitstream
				uint64_t alignment_bits = (0 - br.count) & 0x07;
				discard_bits(&br, alignment_bits);

				// Read table size/offset
				uint64_t table_offset = read_16_bits(&br);

				// Store current stream position as table pointer
				const uint8_t *table_base = get_stream_byte_address(&br);

				// table_offset comes straight off the stream. Under the old
				// masking this could point anywhere and still appear to work.
				// The high side is checked on its own rather than folded into
				// the ROM_END - table_base difference: if table_base is ever
				// past ROM_END, that difference is a negative ptrdiff_t, and
				// converting it to size_t wraps it to a huge value that would
				// pass the < table_offset test regardless of table_offset.
				if (table_base < rom_image || table_base > ROM_END ||
				    (size_t)(ROM_END - table_base) < table_offset)
				{
					fprintf(stderr,
					        "unpack: Huffman table %d starts past the ROM window\n", i);
					free(out);
					return -1;
				}

				ws.tables[i] = table_base;

				// Seek past this table to the next one
				seek_bitstream(&br, table_base + table_offset);
			}

			if (br.error)
			{
				free(out);
				return -1;
			}

			// ===== MIPS INSTRUCTION DECODING LOOP =====
			// Decode one 32-bit instruction per iteration
			do
			{
				// MIPS instruction fields (will be assembled at the end)
				uint64_t instruction = 0;      // Accumulated instruction bits
				uint64_t immediate = 0;        // 16-bit immediate value (I-type)
				uint64_t rs_field = 0;         // Source register 1 (bits 21-25)
				uint64_t rt_field = 0;         // Source/dest register 2 (bits 16-20)
				uint64_t rd_field = 0;         // Destination register (bits 11-15)
				uint64_t shamt_field = 0;      // Shift amount (bits 6-10)

				// Decode opcode using first Huffman table
				const uint8_t *decoded = ws.tables[TBL_OPCODE];
				decoded = decode_huffman_symbol(decoded, &br);

				// Get opcode value and look up instruction format flags.
				//
				// The cast is not cosmetic. READ8 used to return int8_t, so
				// a symbol byte of 0x80 or above sign-extended into a huge
				// value and indexed the opcode table backwards - which the
				// 29-bit mask absorbed and a real buffer would not. Every
				// other symbol read here already casts. No shipped ROM
				// decodes an opcode above 63, so this is inert for real
				// input and the baselines prove it.
				uint64_t opcode = *decoded;
				uint64_t format_flags =
					load_le16(rom_image + current_rom->opcode_table + opcode * 2);

				// Handle special opcode cases
				if (opcode == 0)
				{
					// Opcode 0: SPECIAL instructions (R-type) - decode function code
					decoded = ws.tables[TBL_FUNCT];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = *decoded;

					// Look up format flags for this function code
					format_flags =
						load_le16(rom_image + current_rom->funct_table + immediate * 2);

					if (format_flags == 0)
					{
						// Raw 26-bit value (shifted left 6 for position)
						uint64_t raw_bits = read_bits(&br, 0x14);
						instruction = instruction | (raw_bits << 0x6);
						format_flags = 0x0;
					}
				}
				else if (opcode == 1)
				{
					// Opcode 1: REGIMM instructions - decode rt field for branch type
					decoded = ws.tables[TBL_REGIMM];
					decoded = decode_huffman_symbol(decoded, &br);
					rt_field = *decoded;

					format_flags =
						load_le16(rom_image + current_rom->regimm_table + rt_field * 2);

					if (format_flags == 0)
					{
						// Decode rs and immediate for branch
						decoded = ws.tables[TBL_EXT_REG];
						decoded = decode_huffman_symbol(decoded, &br);
						rs_field = rs_field | *decoded;

						decoded = ws.tables[TBL_BR_IMM_HI];
						decoded = decode_huffman_symbol(decoded, &br);
						immediate = immediate | (*decoded << 0x8);

						decoded = ws.tables[TBL_BR_IMM_LO];
						decoded = decode_huffman_symbol(decoded, &br);
						immediate = immediate | *decoded;
					}
				}
				else if (format_flags == 0xffff)
				{
					// Extended format: need additional decoding
					decoded = ws.tables[TBL_EXT_CODE];
					decoded = decode_huffman_symbol(decoded, &br);
					uint64_t ext_code = *decoded;

					format_flags =
						load_le16(rom_image + current_rom->extended_table + ext_code * 2);
					rs_field = rs_field | ext_code;

					if (format_flags == 0)
					{
						// Full decoding needed
						decoded = ws.tables[TBL_EXT_REG];
						decoded = decode_huffman_symbol(decoded, &br);
						rt_field = rt_field | *decoded;

						decoded = ws.tables[TBL_BR_IMM_HI];
						decoded = decode_huffman_symbol(decoded, &br);
						immediate = immediate | (*decoded << 0x08);

						decoded = ws.tables[TBL_BR_IMM_LO];
						decoded = decode_huffman_symbol(decoded, &br);
						immediate = immediate | *decoded;
					}
					else if (format_flags == 0xc00)
					{
						// Coprocessor instruction format
						decoded = ws.tables[TBL_COP_RT];
						decoded = decode_huffman_symbol(decoded, &br);
						rt_field = rt_field | *decoded;

						if (rt_field >= 0x4)
						{
							decoded = ws.tables[TBL_BR_IMM_HI];
							decoded = decode_huffman_symbol(decoded, &br);
							immediate = immediate | (*decoded << 0x8);

							decoded = ws.tables[TBL_BR_IMM_LO];
							decoded = decode_huffman_symbol(decoded, &br);
							format_flags = 0;
							immediate = immediate | *decoded;
						}
					}
					else if (format_flags == 0xffff)
					{
						// Raw 21-bit value
						uint64_t raw_bits = read_bits(&br, 0x15);
						instruction = instruction | raw_bits;
						format_flags = 0;
					}
				}
				else if (format_flags == 0)
				{
					// Raw 26-bit jump target
					uint64_t raw_bits = read_bits(&br, 0x1a);
					instruction = instruction | raw_bits;
					format_flags = 0;
				}
				else if (format_flags == 0x4000)
				{
					// JR $ra (return) - hardcoded instruction
					//
					// KEEP. Unreached by every ROM we ship, because none of
					// their opcode/funct/regimm/extended tables holds 0x4000
					// - but format_flags here is read out of the ROM image
					// at runtime, not from a constant, so "no current input
					// reaches it" is not the same as "no input can". Deleting
					// it would silently change how this decoder handles a
					// table that does use the flag, and byte-compatibility
					// with the original is the whole point of this file.
					//
					// pack.c's twin branches ARE permanently dead, since pack
					// builds from its own compile-time tables. The reasoning
					// that justifies removing one does not transfer here.
					instruction = 0x03e00008;  // jr $ra
					opcode = 0;
				}
				else if (format_flags == 0x2000)
				{
					// NOP instruction. Kept for the same reason as 0x4000
					// above: runtime table data, not a compile-time constant.
					instruction = 0;
					opcode = 0;
				}

				// ===== DECODE INSTRUCTION FIELDS BASED ON FORMAT FLAGS =====

				// Decode rt field (bits 16-20)
				if ((format_flags & 0x4) != 0)
				{
					// Decode rt with MTE table 1
					decoded = ws.tables[TBL_RT_MTE];
					decoded = decode_huffman_symbol(decoded, &br);
					uint64_t mtf_index = *decoded;
					int64_t reg_value = mtf_decode_reg(ws.mtf[0], mtf_index);
					if (reg_value < 0)
					{
						free(out);
						return -1;
					}
					rt_field = rt_field | (uint64_t)reg_value;
				}
				else if ((format_flags & 0x8) != 0)
				{
					// Decode rt directly (no MTE)
					decoded = ws.tables[TBL_DIRECT];
					decoded = decode_huffman_symbol(decoded, &br);
					rt_field = rt_field | *decoded;
				}

				// Decode rs field (bits 21-25)
				if ((format_flags & 0x1) != 0)
				{
					if ((format_flags & 0x200) != 0)
					{
						// Decode rs with MTE table 2
						decoded = ws.tables[TBL_RS_MTE2];
						decoded = decode_huffman_symbol(decoded, &br);
						uint64_t mtf_index = *decoded;
						int64_t reg_value = mtf_decode_reg(ws.mtf[1], mtf_index);
						if (reg_value < 0)
						{
							free(out);
							return -1;
						}
						rs_field = rs_field | (uint64_t)reg_value;
					}
					else
					{
						// Decode rs with MTE table 1
						decoded = ws.tables[TBL_RS_MTE];
						decoded = decode_huffman_symbol(decoded, &br);
						uint64_t mtf_index = *decoded;
						int64_t reg_value = mtf_decode_reg(ws.mtf[0], mtf_index);
						if (reg_value < 0)
						{
							free(out);
							return -1;
						}
						rs_field = rs_field | (uint64_t)reg_value;
					}
				}
				else if ((format_flags & 0x2) != 0)
				{
					// Decode rs directly (no MTE)
					decoded = ws.tables[TBL_DIRECT];
					decoded = decode_huffman_symbol(decoded, &br);
					rs_field = rs_field | *decoded;
				}

				// Decode rd field (bits 11-15)
				if ((format_flags & 0x10) != 0)
				{
					// Decode rd with MTE table 1
					decoded = ws.tables[TBL_RD_MTE];
					decoded = decode_huffman_symbol(decoded, &br);
					uint64_t mtf_index = *decoded;
					int64_t reg_value = mtf_decode_reg(ws.mtf[0], mtf_index);
					if (reg_value < 0)
					{
						free(out);
						return -1;
					}
					rd_field = rd_field | (uint64_t)reg_value;
				}
				else if ((format_flags & 0x20) != 0)
				{
					// Decode rd directly (no MTE)
					decoded = ws.tables[TBL_DIRECT];
					decoded = decode_huffman_symbol(decoded, &br);
					rd_field = rd_field | *decoded;
				}

				// Decode shamt field (bits 6-10)
				if ((format_flags & 0x40) != 0)
				{
					// Decode shamt with specialized table
					decoded = ws.tables[TBL_SHAMT];
					decoded = decode_huffman_symbol(decoded, &br);
					shamt_field = *decoded;
				}
				else if ((format_flags & 0x80) != 0)
				{
					// Decode shamt directly
					decoded = ws.tables[TBL_DIRECT];
					decoded = decode_huffman_symbol(decoded, &br);
					shamt_field = *decoded;
				}

				// Decode jump target (26-bit, split into 4 parts)
				if ((format_flags & 0x100) != 0)
				{
					decoded = ws.tables[TBL_TARGET_0];
					decoded = decode_huffman_symbol(decoded, &br);
					instruction = instruction | (*decoded << 0x14);

					decoded = ws.tables[TBL_TARGET_1];
					decoded = decode_huffman_symbol(decoded, &br);
					instruction = instruction | (*decoded << 0xe);

					decoded = ws.tables[TBL_TARGET_2];
					decoded = decode_huffman_symbol(decoded, &br);
					instruction = instruction | (*decoded << 0x7);

					decoded = ws.tables[TBL_TARGET_3];
					decoded = decode_huffman_symbol(decoded, &br);
					instruction = instruction | *decoded;
				}

				// Decode 16-bit immediate (3 different encoding types)
				//
				// The shape here is load-bearing: 0x400 and 0x800 are
				// standalone `if`s, and the 0x1000 arm below is the `else` of
				// the 0xc00 test alone. That is what the original decoder does,
				// and format_flags is read out of the ROM at runtime, so it is
				// not ours to simplify. pack.c's matching block is a single
				// else-if chain, which differs only for a flags value no
				// shipped table holds; see the note there.
				if ((format_flags & 0xc00) == 0x400)
				{
					// Immediate type 1
					decoded = ws.tables[TBL_IMM1_HI];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | (*decoded << 0x8);

					decoded = ws.tables[TBL_IMM1_LO];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | *decoded;
				}

				if ((format_flags & 0xc00) == 0x800)
				{
					// Immediate type 2
					decoded = ws.tables[TBL_IMM2_HI];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | (*decoded << 0x8);

					decoded = ws.tables[TBL_IMM2_LO];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | *decoded;
				}

				if ((format_flags & 0xc00) == 0xc00)
				{
					// Immediate type 3
					decoded = ws.tables[TBL_IMM3_HI];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | (*decoded << 0x8);

					decoded = ws.tables[TBL_IMM3_LO];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | *decoded;
				}
				else if ((format_flags & 0x1000) != 0)
				{
					// Immediate direct (no high byte)
					decoded = ws.tables[TBL_DIRECT];
					decoded = decode_huffman_symbol(decoded, &br);
					immediate = immediate | *decoded;
				}

				// ===== ASSEMBLE THE 32-BIT MIPS INSTRUCTION =====
				// Format: [opcode:6][rs:5][rt:5][rd:5][shamt:5][funct:6] or
				//         [opcode:6][rs:5][rt:5][immediate:16]
				opcode = opcode << 26;           // Opcode to bits 26-31
				instruction = instruction | opcode;
				instruction = instruction | immediate;
				rs_field = rs_field << 21;       // rs to bits 21-25
				instruction = instruction | rs_field;
				rt_field = rt_field << 16;       // rt to bits 16-20
				instruction = instruction | rt_field;
				rd_field = rd_field << 11;       // rd to bits 11-15
				instruction = instruction | rd_field;
				shamt_field = shamt_field << 6;  // shamt to bits 6-10
				instruction = instruction | shamt_field;

				// One test per instruction is enough: every guard returns a
				// safe value, so nothing reads out of bounds between the trip
				// and here.
				if (br.error)
				{
					free(out);
					return -1;
				}

				// Write the reconstructed instruction to destination
				decompressed_size = decompressed_size - 4;
				store_le32(out + written, (uint32_t)instruction);
				written = written + 4;
			} while (decompressed_size != 0);
		}

		// Dump decompressed file to disk. A zero-length file passes out == NULL
		// here, which write_file() handles: it skips the fwrite entirely.
		int dumped = dump_bin(output_dir, total_files - files_remaining,
		                      out, (uint32_t)original_size, load_addr);
		free(out);
		if (dumped != 0)
		{
			return -1;
		}
		files_remaining = files_remaining - 1;
	} while (files_remaining != 0);

	return 0;
}

int kipack_unpack(const char *rom_path, const char *out_dir, kipack_variant_t variant)
{
	if (variant == KIPACK_AUTO)
		variant = kipack_variant_sniff(rom_path);

	// kipack_unpack_main can only reach here with a parsed variant, but this
	// is the public entry point: a caller passing a cast integer - or a new
	// enumerator added without a matching rom_offsets[] row - would otherwise
	// index past the array and decode against whatever follows it in .rodata.
	if ((int)variant < 0 || (int)variant >= (int)ROM_OFFSETS_COUNT)
	{
		fprintf(stderr, "unpack: variant %d out of range\n", (int)variant);
		return -1;
	}

	current_rom = &rom_offsets[variant];

	if (load_rom(rom_path) != 0)
		return -1;

	return decompress_rom(out_dir);
}

int kipack_unpack_main(int argc, char **argv)
{
	kipack_variant_t variant = KIPACK_AUTO;
	int i = 1;

	if (i + 1 < argc && strcmp(argv[i], "-t") == 0)
	{
		variant = kipack_variant_parse(argv[i + 1]);
		if (variant == KIPACK_AUTO)
		{
			fprintf(stderr, "unpack: unknown variant '%s'\n", argv[i + 1]);
			return 1;
		}
		i += 2;
	}

	if (argc - i != 2)
	{
		fprintf(stderr, "usage: unpack [-t ki1|ki1-p47|ki2|packed] <rom> <out_dir>\n");
		return 1;
	}

	return kipack_unpack(argv[i], argv[i + 1], variant) == 0 ? 0 : 1;
}