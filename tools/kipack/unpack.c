#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kipack.h"
#include "unpack.h"

#define ADDR32(a) ((uint64_t)(a) & 0xFFFFFFFF)
#define ADDR64(a) ((uint64_t)(a) & 0xFFFFFFFFFFFFFFFF)

static uint8_t ram[0x20000000];

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

static int dump_bin(const char *output, int id, uint8_t *src, uint32_t length)
{
	char filename[256] = {0};
	char addr_filename[256] = {0};
	snprintf(filename, sizeof(filename), "%s/rom-%d.bin", output, id);
	snprintf(addr_filename, sizeof(addr_filename), "%s/rom-%d.addr", output, id);

	if (write_file(filename, src, length) != 0)
	{
		return -1;
	}

	uint32_t addr = src - ram;
	if (write_file(addr_filename, &addr, sizeof(addr)) != 0)
	{
		return -1;
	}

	printf("unpacked rom-%d @ 0x%08lx (length = 0x%x)\n",
	       id, (unsigned long)(src - ram), length);
	return 0;
}

static uint32_t sram(uint32_t offset)
{
	return offset + 0;
}

static uint32_t rom(uint32_t offset)
{
	return offset + 0x1fc00000;
}

static uint32_t dram(uint32_t offset)
{
	return offset + 0x8000000;
}

static uint32_t uncached_ram(uint64_t offset)
{
	return offset + 0xa0000000UL;
}

static uint32_t cached_ram(uint64_t offset)
{
	return offset + 0x80000000UL;
}

static void *pram(void *p)
{
	return (void *)(((uint64_t)p & 0x1FFFFFFF) + ram);
}

static int8_t READ8(uint32_t base, int32_t offset)
{
	int8_t *p = (int8_t *)pram((void *)ADDR64(base));
	return *(p + offset);
}

static uint16_t READ16(uint32_t base, int32_t offset)
{
	uint8_t *p = (uint8_t *)pram((void *)ADDR64(base));
	return *(uint16_t *)(p + offset);
}

static uint32_t READ32(uint32_t base, int32_t offset)
{
	uint8_t *p = (uint8_t *)pram((void *)ADDR64(base));
	return *(uint32_t *)(p + offset);
}

static uint64_t READ64(uint32_t base, int32_t offset)
{
	uint8_t *p = (uint8_t *)pram((void *)ADDR64(base));
	return *(uint64_t *)(p + offset);
}

static void WRITE8(uint32_t base, int32_t offset, uint8_t value)
{
	uint8_t *p = (uint8_t *)pram((void *)ADDR64(base));
	*(p + offset) = value;
}

static void WRITE32(uint32_t base, int32_t offset, uint32_t value)
{
	uint8_t *p = (uint8_t *)pram((void *)ADDR64(base));
	*(uint32_t *)(p + offset) = value;
}

/* Size of the ROM window the boot ROM is mapped into. */
#define ROM_WINDOW_SIZE 0x80000

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
	// whatever the previous ROM left behind.
	memset(ram + rom(0x00), 0, ROM_WINDOW_SIZE);

	size_t got = fread(ram + rom(0x00), 1, ROM_WINDOW_SIZE, fd);
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
 * Bitstream reader state structure.
 *
 * This structure holds the persistent state of the bitstream reader,
 * allowing it to be saved to and loaded from memory. This is used by
 * the bootrom to maintain decompression state across function calls.
 */
typedef struct
{
	uint64_t bit_buffer;      // 64-bit buffer holding prefetched bits from stream
	uint32_t stream_ptr;      // Current position in the byte stream (points 7 bytes ahead)
	int8_t bits_remaining;    // Count of bits left in buffer (negative, -56 when full)
} bitstream_state_t;

/**
 * Initializes a bitstream state structure from a source address.
 *
 * Reads the first 64 bits from the compressed data and sets up the
 * bitstream reader state for subsequent read operations.
 *
 * @param source_addr  Address of the compressed data to read from
 * @param state_addr   Address where the bitstream state structure will be stored
 */
static void init_bitstream_state(uint32_t source_addr, uint32_t state_addr)
{
	bitstream_state_t *state = pram((void *)ADDR64(state_addr));

	// Load first 64 bits from compressed data into the bit buffer
	state->bit_buffer = READ64(source_addr, 0x0);

	// Set stream pointer to 7 bytes ahead (standard lookahead offset)
	state->stream_ptr = ADDR32(source_addr + 0x7);

	// Initialize bits_remaining to -56 (buffer is full with 56 usable bits)
	state->bits_remaining = -56;  // -0x38
}

/**
 * Loads bitstream state from a structure into local variables.
 *
 * Retrieves the saved bitstream reader state from memory so it can be
 * used for subsequent read operations.
 *
 * @param state_addr      Address of the bitstream state structure
 * @param bit_buffer      Pointer to receive the 64-bit buffer value
 * @param stream_ptr      Pointer to receive the stream position
 * @param bits_remaining  Pointer to receive the bits remaining count
 */
static void load_bitstream_state(uint8_t *state_addr, uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
	bitstream_state_t *state = pram(state_addr);

	*bit_buffer = state->bit_buffer;
	*stream_ptr = state->stream_ptr;
	*bits_remaining = state->bits_remaining;
}

/**
 * Reads a specified number of bits from the bitstream.
 *
 * @param bit_buffer     Pointer to the 64-bit buffer holding prefetched bits
 * @param stream_ptr     Pointer to the current position in the byte stream
 * @param bits_remaining Pointer to count of bits left in buffer (negative value, -56 when full)
 * @param num_bits       Number of bits to extract (1-32)
 * @return               The extracted bits as a uint64_t value
 */
static uint64_t read_bits(uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining, uint8_t num_bits)
{
    // Read next 64 bits from stream (lookahead)
    uint64_t next_bits = READ64(*stream_ptr, 0x0);

    // Convert negative remaining count to positive shift amount
    int8_t shift_amount = 0 - *bits_remaining;

    // Shift lookahead bits and merge into buffer
    uint64_t shifted_bits = next_bits << shift_amount;
    *bit_buffer = *bit_buffer | shifted_bits;

    // Calculate how many bits we'll be short after this read
    *bits_remaining = num_bits - shift_amount;

    // Create bitmask for extracting num_bits (e.g., num_bits=8 -> mask=0xFF)
    uint64_t bitmask = (1ULL << num_bits) - 1;

    // Extract the requested bits from buffer
    uint64_t result = *bit_buffer & bitmask;

    // Check if we need to refill the buffer
    if (*bits_remaining <= 0)
    {
        // We have enough bits - just shift out the consumed bits
        *bit_buffer = *bit_buffer >> num_bits;
        return result;
    }

    // Need to refill: advance stream pointer by 7 bytes
    *stream_ptr = *stream_ptr + 0x7;

    // Reload buffer from the lookahead, shifted to align remaining bits
    *bit_buffer = next_bits >> *bits_remaining;

    // Reset remaining count: we now have 56 bits minus what we still need
    *bits_remaining = *bits_remaining - 56;  // -0x38

    return result;
}

static uint64_t read_16_bits(uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
    return read_bits(bit_buffer, stream_ptr, bits_remaining, 16);
}

static uint64_t read_8_bits(uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
    return read_bits(bit_buffer, stream_ptr, bits_remaining, 8);
}

static uint64_t read_32_bits(uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
    return read_bits(bit_buffer, stream_ptr, bits_remaining, 32);
}

/**
 * Initializes two 32-byte Move-to-Front (MTF) tables with descending values.
 *
 * Each table is filled with values [31, 30, 29, ..., 1, 0] at indices [0, 1, 2, ..., 30, 31].
 * Table 1 is at offset 0x00, Table 2 is at offset 0x20 (32 bytes apart).
 *
 * These tables are used by the MTF transform during decompression, where frequently
 * used symbols are moved to the front of the list for better compression.
 *
 * @param table_base  Base address for the two MTF tables (each 32 bytes)
 */
static void init_mtf_tables(uint8_t *table_base)
{
	uint8_t *table_ptr = pram(table_base);
	int8_t value = 31;  // Start with highest value (0x1F)

	do
	{
		*table_ptr = value;            // Table 1: table1[i] = 31-i
		*(table_ptr + 0x20) = value;   // Table 2: table2[i] = 31-i (32 bytes offset)
		value = value - 1;
		table_ptr = table_ptr + 1;
	} while (value >= 0);  // Loop 32 times (values 31 down to 0)
}

/**
 * Discards a specified number of bits from the bitstream without returning them.
 *
 * This is typically used for byte alignment before reading byte-aligned data.
 * The caller computes bits_to_skip as: (0 - bits_remaining) & 0x07
 * which gives the number of bits to skip to reach the next byte boundary.
 *
 * @param bit_buffer     Pointer to the 64-bit buffer holding prefetched bits
 * @param stream_ptr     Pointer to the current position in the byte stream
 * @param bits_remaining Pointer to count of bits left in buffer (negative value)
 * @param bits_to_skip   Number of bits to discard (typically 0-7 for alignment)
 */
static void discard_bits(uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining, uint64_t bits_to_skip)
{
	// Consume the specified bits from the remaining count
	*bits_remaining = *bits_remaining + bits_to_skip;

	// Shift out the discarded bits from the buffer
	*bit_buffer = *bit_buffer >> bits_to_skip;

	// Check if we still have bits in the buffer
	if (*bits_remaining <= 0)
	{
		return;  // Still have bits available, done
	}

	// Need to refill the buffer
	*bit_buffer = READ64(*stream_ptr, 0x0);
	*stream_ptr = *stream_ptr + 0x7;
	*bit_buffer = *bit_buffer >> *bits_remaining;
	*bits_remaining = *bits_remaining - 56;  // -0x38
}

/**
 * Calculates the byte-aligned base address from the bitstream state.
 *
 * Given the current stream pointer and bits remaining, this function computes
 * the actual byte position in the stream by accounting for bits that haven't
 * been consumed yet. Since bits_remaining is negative (e.g., -56 when full),
 * negating it gives the number of available bits, dividing by 8 gives bytes.
 *
 * Formula: byte_address = stream_ptr - ((-bits_remaining) / 8)
 *
 * @param stream_ptr     Current position in the byte stream
 * @param bits_remaining Count of bits left in buffer (negative value)
 * @return               The byte-aligned base address
 */
static uint64_t get_stream_byte_address(uint32_t stream_ptr, uint8_t bits_remaining)
{
	// Convert negative bits_remaining to positive bits available
	uint64_t bits_available = (0 - bits_remaining) & 0xFF;

	// Convert bits to bytes (divide by 8)
	uint64_t bytes_available = bits_available >> 3;

	// Subtract from stream pointer to get base address
	return stream_ptr - bytes_available;
}

/**
 * Seeks the bitstream reader to a specific byte address.
 *
 * Reinitializes the bitstream reader state to start reading from a new
 * position. This is used to jump to different locations within the
 * compressed data stream.
 *
 * @param byte_address   The byte address to seek to
 * @param bit_buffer     Pointer to the 64-bit buffer (will be filled with data at address)
 * @param stream_ptr     Pointer to the stream position (will be set to address + 7)
 * @param bits_remaining Pointer to bits counter (will be reset to -56, indicating full buffer)
 */
static void seek_bitstream(uint64_t byte_address, uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
	// Load 64 bits from the target address into the bit buffer
	*bit_buffer = READ64(byte_address, 0x0);

	// Reset bits_remaining to -56 (buffer is full with 56 usable bits)
	*bits_remaining = -56;  // -0x38

	// Set stream pointer to 7 bytes ahead (standard lookahead offset)
	*stream_ptr = byte_address + 0x7;
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
 * @param table_ptr      Starting address in the Huffman tree table
 * @param bit_buffer     Pointer to the 64-bit buffer holding prefetched bits
 * @param stream_ptr     Pointer to current position in the byte stream
 * @param bits_remaining Pointer to count of bits left in buffer
 * @return               Address of the decoded symbol in the table
 */
static uint64_t decode_huffman_symbol(uint64_t table_ptr, uint64_t *bit_buffer, uint32_t *stream_ptr, int8_t *bits_remaining)
{
	uint64_t decision_bit;
	do
	{
		// Read skip count from current node (signed byte)
		int16_t skip_count = READ8(table_ptr, 0x0);
		table_ptr = table_ptr + 1;

		// Get next bit from bitstream for branch decision
		decision_bit = *bit_buffer & 0x1;

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
			skip_count = skip_count & 0x7f;              // Extract high 7 bits
			skip_count = skip_count << 8;                // Shift to high byte
			uint8_t low_byte = READ8(table_ptr, 0x0);    // Read low 8 bits
			skip_count = skip_count + low_byte;          // Combine into 15-bit value
			table_ptr = table_ptr + 1;
		}

		// Consume one bit from the bitstream
		*bits_remaining = *bits_remaining + 1;
		*bit_buffer = *bit_buffer >> 1;

		// Refill bit buffer if exhausted
		if (*bits_remaining > 0)
		{
			*bit_buffer = READ64(*stream_ptr, 0x0);
			*stream_ptr = *stream_ptr + 0x7;
			*bit_buffer = *bit_buffer >> *bits_remaining;
			*bits_remaining = *bits_remaining - 56;
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
 * Move-to-Front (MTF) update operation on a 32-byte table.
 *
 * Finds the given symbol in the table and moves it to the front,
 * shifting all preceding elements down by one position.
 *
 * Example: If table is [A, B, C, D, E] and symbol is 'D':
 *   1. Find 'D' at index 3
 *   2. Shift elements: [A, B, C, _, E] -> [A, B, _, C, E] -> [A, _, B, C, E] -> [_, A, B, C, E]
 *   3. Insert 'D' at front: [D, A, B, C, E]
 *
 * This keeps frequently used symbols near the front, improving compression
 * when combined with variable-length coding.
 *
 * Returns 0 on success, -1 when the symbol is not one of the table's 32
 * entries.
 *
 * @param table_ptr  Pointer to the 32-byte MTF table
 * @param symbol     The symbol value to find and move to front
 */
static int mtf_move_to_front(uint64_t table_ptr, uint64_t symbol)
{
	int64_t position = -31;  // Start searching from position 0 (will count up to 0)
	int i;

	// PHASE 1: Search for the symbol in the table.
	//
	// Bounded at the table's 32 entries. `symbol` is read out of the workspace
	// at an index the bitstream decoded, so a corrupt stream can ask for a
	// value that is in no table at all - and the comparison used to be int8_t
	// against a zero-extended uint64_t, which no entry >= 0x80 could ever
	// satisfy. Unbounded, the scan then ran off the end of the 32-byte table
	// and through the rest of the workspace until some byte happened to match,
	// or spun forever.
	for (i = 0; i < 32; i++)
	{
		if ((uint8_t)READ8(table_ptr, 0x0) == (uint8_t)symbol)
		{
			break;  // Found the symbol
		}
		position = position + 1;
		table_ptr = table_ptr + 1;
	}
	if (i == 32)
	{
		fprintf(stderr, "unpack: MTF symbol 0x%02x is not in the table\n",
		        (unsigned)(symbol & 0xFF));
		return -1;
	}
	// After loop: table_ptr points to where symbol was found
	//             position is negative if symbol was not at the end

	// PHASE 2: Shift elements backward to make room at front
	do
	{
		if (position >= 0)
		{
			break;  // Reached the front of the table
		}
		int8_t next_value = READ8(table_ptr, 0x1);  // Read element ahead
		position = position + 1;
		table_ptr = table_ptr + 1;
		WRITE8(table_ptr, -1, next_value);  // Shift element backward
	} while (1);

	// PHASE 3: Place the symbol at its new position (front)
	WRITE8(table_ptr, 0x0, symbol);
	return 0;
}

/**
 * Reads the register held at `index` in a 32-entry MTF table, moves it to the
 * table's other end, and returns it.
 *
 * `index` is a symbol the Huffman decoder produced, so it is a full byte and a
 * corrupt table can put it past the 32 entries. Reject that rather than
 * reading - and then reordering around - whatever follows the table in the
 * workspace.
 *
 * @param table_addr  Address of the 32-byte MTF table
 * @param index       Table index decoded from the bitstream
 * @return            The register value, or -1 if the index is out of range
 */
static int64_t mtf_decode_reg(uint64_t table_addr, uint64_t index)
{
	if (index >= 32)
	{
		fprintf(stderr, "unpack: MTF index %lu is past the 32 table entries\n",
		        (unsigned long)index);
		return -1;
	}

	uint64_t value = (uint8_t)READ8(table_addr + index, 0x0);
	if (mtf_move_to_front(table_addr, value) != 0)
	{
		return -1;
	}
	return (int64_t)value;
}

static inline uint64_t decode(uint64_t addr, int32_t offset, uint8_t shift, uint64_t *s0, uint32_t *s1, int8_t *s2)
{
	uint64_t v0 = READ32(addr, offset);
	v0 = decode_huffman_symbol(v0, s0, s1, s2);
	v0 = (uint8_t)READ8(v0, 0x0);
	return v0 << shift;
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
 * 4. Uses Huffman coding + Move-to-Front transform for compression
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
	// Memory addresses for decompression workspace
	uint64_t video_ram = uncached_ram(sram(0x30000));
	uint64_t bitstream_state_addr = cached_ram(dram(0x7fff00));
	uint64_t compressed_data_addr = uncached_ram(rom(current_rom->compressed_data));
	uint64_t workspace_addr = cached_ram(dram(0x7fff10));
	uint64_t entry_point = 0;  // Will hold address of first decompressed file (jump target)

	// Initialize bitstream state structure in RAM from compressed ROM data
	init_bitstream_state(compressed_data_addr, bitstream_state_addr);

	// Bitstream reader state variables
	uint64_t bit_buffer;
	uint32_t stream_ptr;
	int8_t bits_remaining;

	// Load bitstream state from RAM into local variables
	load_bitstream_state((uint8_t *)bitstream_state_addr, &bit_buffer, &stream_ptr, &bits_remaining);

	// Verify magic number "br" (0x7262 = 'b' 'r' for "binary ROM")
	uint64_t magic = read_16_bits(&bit_buffer, &stream_ptr, &bits_remaining);
	if (magic != KIPACK_MAGIC)
	{
		fprintf(stderr, "unpack: invalid magic: expected 0x%x, got 0x%lx\n",
		        KIPACK_MAGIC, (unsigned long)magic);
		return -1;
	}

	// Read number of compressed files
	uint64_t file_count = read_8_bits(&bit_buffer, &stream_ptr, &bits_remaining);

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
		uint64_t file_type = read_8_bits(&bit_buffer, &stream_ptr, &bits_remaining);
		if (file_type != 0)
		{
			fprintf(stderr, "unpack: unsupported file type %lu\n",
			        (unsigned long)file_type);
			return -1;
		}

		// Read decompressed size and destination address
		uint64_t decompressed_size = read_32_bits(&bit_buffer, &stream_ptr, &bits_remaining);

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

		uint64_t dest_addr = read_32_bits(&bit_buffer, &stream_ptr, &bits_remaining);
		dest_addr = (dest_addr << 32) >> 32;  // Sign-extend to 64 bits

		// Remember entry point (first file's destination = boot address)
		if (entry_point == 0)
		{
			entry_point = dest_addr;
		}

		if (decompressed_size != 0)
		{
			// Initialize Move-to-Front tables for register encoding
			init_mtf_tables((uint8_t *)workspace_addr);

			// Load 24 Huffman table pointers (offsets 0x40-0x9C in workspace)
			uint64_t table_ptr_addr = workspace_addr + 0x40;
			uint64_t table_ptr_end = workspace_addr + 0xa0;

			do
			{
				// Byte-align the bitstream
				uint64_t alignment_bits = (0 - bits_remaining) & 0x07;
				discard_bits(&bit_buffer, &stream_ptr, &bits_remaining, alignment_bits);

				// Read table size/offset
				uint64_t table_offset = read_16_bits(&bit_buffer, &stream_ptr, &bits_remaining);

				// Store current stream position as table pointer
				uint64_t table_base = get_stream_byte_address(stream_ptr, bits_remaining);
				WRITE32(table_ptr_addr, 0x0, table_base);

				// Seek past this table to the next one
				seek_bitstream(table_base + table_offset, &bit_buffer, &stream_ptr, &bits_remaining);
				table_ptr_addr = table_ptr_addr + 0x4;
			} while (table_ptr_addr != table_ptr_end);

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
				uint64_t decoded = READ32(workspace_addr, 0x40);
				decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);

				// Get opcode value and look up instruction format flags
				uint64_t opcode = READ8(decoded, 0x0);
				uint64_t format_flags = uncached_ram(rom(current_rom->opcode_table));
				format_flags = format_flags + opcode + opcode;  // Index by opcode * 2
				format_flags = READ16(format_flags, 0x0);

				// Handle special opcode cases
				if (opcode == 0)
				{
					// Opcode 0: SPECIAL instructions (R-type) - decode function code
					decoded = READ32(workspace_addr, 0x44);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = (uint8_t)READ8(decoded, 0x0);

					// Look up format flags for this function code
					format_flags = uncached_ram(rom(current_rom->funct_table));
					format_flags = format_flags + immediate + immediate;
					format_flags = READ16(format_flags, 0x0);

					if (format_flags == 0)
					{
						// Raw 26-bit value (shifted left 6 for position)
						uint64_t raw_bits = read_bits(&bit_buffer, &stream_ptr, &bits_remaining, 0x14);
						instruction = instruction | (raw_bits << 0x6);
						format_flags = 0x0;
					}
				}
				else if (opcode == 1)
				{
					// Opcode 1: REGIMM instructions - decode rt field for branch type
					decoded = READ32(workspace_addr, 0x48);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					rt_field = (uint8_t)READ8(decoded, 0x0);

					format_flags = uncached_ram(rom(current_rom->regimm_table));
					format_flags = format_flags + rt_field + rt_field;
					format_flags = READ16(format_flags, 0x0);

					if (format_flags == 0)
					{
						// Decode rs and immediate for branch
						decoded = READ32(workspace_addr, 0x94);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						rs_field = rs_field | (uint8_t)READ8(decoded, 0x0);

						decoded = READ32(workspace_addr, 0x64);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x8);

						decoded = READ32(workspace_addr, 0x68);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						immediate = immediate | (uint8_t)READ8(decoded, 0x0);
					}
				}
				else if (format_flags == 0xffff)
				{
					// Extended format: need additional decoding
					decoded = READ32(workspace_addr, 0x98);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					uint64_t ext_code = (uint8_t)READ8(decoded, 0x0);

					format_flags = uncached_ram(rom(current_rom->extended_table));
					format_flags = format_flags + ext_code + ext_code;
					format_flags = READ16(format_flags, 0x0);
					rs_field = rs_field | ext_code;

					if (format_flags == 0)
					{
						// Full decoding needed
						decoded = READ32(workspace_addr, 0x94);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						rt_field = rt_field | (uint8_t)READ8(decoded, 0x0);

						decoded = READ32(workspace_addr, 0x64);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x08);

						decoded = READ32(workspace_addr, 0x68);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						immediate = immediate | (uint8_t)READ8(decoded, 0x0);
					}
					else if (format_flags == 0xc00)
					{
						// Coprocessor instruction format
						decoded = READ32(workspace_addr, 0x9c);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						rt_field = rt_field | (uint8_t)READ8(decoded, 0x0);

						if (rt_field >= 0x4)
						{
							decoded = READ32(workspace_addr, 0x64);
							decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
							immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x8);

							decoded = READ32(workspace_addr, 0x68);
							decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
							format_flags = 0;
							immediate = immediate | (uint8_t)READ8(decoded, 0x0);
						}
					}
					else if (format_flags == 0xffff)
					{
						// Raw 21-bit value
						uint64_t raw_bits = read_bits(&bit_buffer, &stream_ptr, &bits_remaining, 0x15);
						instruction = instruction | raw_bits;
						format_flags = 0;
					}
				}
				else if (format_flags == 0)
				{
					// Raw 26-bit jump target
					uint64_t raw_bits = read_bits(&bit_buffer, &stream_ptr, &bits_remaining, 0x1a);
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
					// Decode rt with MTF table 1
					decoded = READ32(workspace_addr, 0x50);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					uint64_t mtf_index = (uint8_t)READ8(decoded, 0x0);
					int64_t reg_value = mtf_decode_reg(workspace_addr + 0x0, mtf_index);
					if (reg_value < 0)
						return -1;
					rt_field = rt_field | (uint64_t)reg_value;
				}
				else if ((format_flags & 0x8) != 0)
				{
					// Decode rt directly (no MTF)
					decoded = READ32(workspace_addr, 0x60);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					rt_field = rt_field | (uint8_t)READ8(decoded, 0x0);
				}

				// Decode rs field (bits 21-25)
				if ((format_flags & 0x1) != 0)
				{
					if ((format_flags & 0x200) != 0)
					{
						// Decode rs with MTF table 2
						decoded = READ32(workspace_addr, 0x5c);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						uint64_t mtf_index = (uint8_t)READ8(decoded, 0x0);
						int64_t reg_value = mtf_decode_reg(workspace_addr + 0x20, mtf_index);
						if (reg_value < 0)
							return -1;
						rs_field = rs_field | (uint64_t)reg_value;
					}
					else
					{
						// Decode rs with MTF table 1
						decoded = READ32(workspace_addr, 0x4c);
						decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
						uint64_t mtf_index = (uint8_t)READ8(decoded, 0x0);
						int64_t reg_value = mtf_decode_reg(workspace_addr + 0x0, mtf_index);
						if (reg_value < 0)
							return -1;
						rs_field = rs_field | (uint64_t)reg_value;
					}
				}
				else if ((format_flags & 0x2) != 0)
				{
					// Decode rs directly (no MTF)
					decoded = READ32(workspace_addr, 0x60);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					rs_field = rs_field | (uint8_t)READ8(decoded, 0x0);
				}

				// Decode rd field (bits 11-15)
				if ((format_flags & 0x10) != 0)
				{
					// Decode rd with MTF table 1
					decoded = READ32(workspace_addr, 0x54);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					uint64_t mtf_index = (uint8_t)READ8(decoded, 0x0);
					int64_t reg_value = mtf_decode_reg(workspace_addr + 0x0, mtf_index);
					if (reg_value < 0)
						return -1;
					rd_field = rd_field | (uint64_t)reg_value;
				}
				else if ((format_flags & 0x20) != 0)
				{
					// Decode rd directly (no MTF)
					decoded = READ32(workspace_addr, 0x60);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					rd_field = rd_field | (uint8_t)READ8(decoded, 0x0);
				}

				// Decode shamt field (bits 6-10)
				if ((format_flags & 0x40) != 0)
				{
					// Decode shamt with specialized table
					decoded = READ32(workspace_addr, 0x58);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					shamt_field = (uint8_t)READ8(decoded, 0x0);
				}
				else if ((format_flags & 0x80) != 0)
				{
					// Decode shamt directly
					decoded = READ32(workspace_addr, 0x60);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					shamt_field = (uint8_t)READ8(decoded, 0x0);
				}

				// Decode jump target (26-bit, split into 4 parts)
				if ((format_flags & 0x100) != 0)
				{
					decoded = READ32(workspace_addr, 0x6c);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					instruction = instruction | ((uint8_t)READ8(decoded, 0x0) << 0x14);

					decoded = READ32(workspace_addr, 0x70);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					instruction = instruction | ((uint8_t)READ8(decoded, 0x0) << 0xe);

					decoded = READ32(workspace_addr, 0x74);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					instruction = instruction | ((uint8_t)READ8(decoded, 0x0) << 0x7);

					decoded = READ32(workspace_addr, 0x78);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					instruction = instruction | (uint8_t)READ8(decoded, 0x0);
				}

				// Decode 16-bit immediate (3 different encoding types)
				if ((format_flags & 0xc00) == 0x400)
				{
					// Immediate type 1
					decoded = READ32(workspace_addr, 0x7c);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x8);

					decoded = READ32(workspace_addr, 0x80);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | (uint8_t)READ8(decoded, 0x0);
				}

				if ((format_flags & 0xc00) == 0x800)
				{
					// Immediate type 2
					decoded = READ32(workspace_addr, 0x84);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x8);

					decoded = READ32(workspace_addr, 0x88);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | (uint8_t)READ8(decoded, 0x0);
				}

				if ((format_flags & 0xc00) == 0xc00)
				{
					// Immediate type 3
					decoded = READ32(workspace_addr, 0x8c);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | ((uint8_t)READ8(decoded, 0x0) << 0x8);

					decoded = READ32(workspace_addr, 0x90);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | (uint8_t)READ8(decoded, 0x0);
				}
				else if ((format_flags & 0x1000) != 0)
				{
					// Immediate direct (no high byte)
					decoded = READ32(workspace_addr, 0x60);
					decoded = decode_huffman_symbol(decoded, &bit_buffer, &stream_ptr, &bits_remaining);
					immediate = immediate | (uint8_t)READ8(decoded, 0x0);
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

				// Write the reconstructed instruction to destination
				decompressed_size = decompressed_size - 4;
				WRITE32(dest_addr, 0x0, instruction);
				dest_addr = dest_addr + 0x4;
			} while (decompressed_size != 0);
		}

		// Dump decompressed file to disk
		if (dump_bin(output_dir, total_files - files_remaining,
		             ram + dram(dest_addr - original_size - 0x88000000),
		             original_size) != 0)
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