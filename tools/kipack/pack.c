/**
 * pack.c - MIPS code compressor for Killer Instinct arcade ROMs
 *
 * This program is the inverse of unpack.c. It takes multiple pairs of
 * .bin and .addr files and compresses them back into the format used
 * by the Killer Instinct bootrom.
 *
 * Compression algorithm:
 *   1. Huffman coding for instruction fields
 *   2. Move-to-Front (MTF) transform for register encoding
 *   3. MIPS instruction-aware field separation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kipack.h"
#include "pack.h"

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define MAX_FILES 16
#define MAX_OUTPUT_SIZE (512 * 1024) // 512KB max compressed output

/* ============================================================================
 * Bitstream Writer
 * ============================================================================ */

typedef struct
{
	uint8_t *data;          // Output buffer
	uint32_t byte_pos;      // Current byte position
	uint8_t bit_pos;        // Current bit position within byte (0-7)
	uint32_t max_size;      // Maximum buffer size
	int error;              // Set on overflow or on a symbol with no code
} bitstream_writer_t;

/**
 * Initializes a bitstream writer.
 */
static void init_bitstream_writer(bitstream_writer_t *bs, uint8_t *buffer, uint32_t max_size)
{
	bs->data = buffer;
	bs->byte_pos = 0;
	bs->bit_pos = 0;
	bs->max_size = max_size;
	bs->error = 0;
	memset(buffer, 0, max_size);
}

/**
 * Writes N bits to the bitstream (LSB first, matching the reader).
 */
static void write_bits(bitstream_writer_t *bs, uint64_t value, uint8_t num_bits)
{
	for (uint8_t i = 0; i < num_bits; i++)
	{
		if (bs->byte_pos >= bs->max_size)
		{
			if (!bs->error)
				fprintf(stderr, "pack: output buffer overflow\n");
			bs->error = 1;
			return;
		}

		// Write bit (LSB first)
		if (value & 1)
		{
			bs->data[bs->byte_pos] |= (1 << bs->bit_pos);
		}

		value >>= 1;
		bs->bit_pos++;

		if (bs->bit_pos == 8)
		{
			bs->bit_pos = 0;
			bs->byte_pos++;
		}
	}
}

/**
 * Aligns the bitstream to the next byte boundary.
 */
static void align_to_byte(bitstream_writer_t *bs)
{
	if (bs->bit_pos != 0)
	{
		bs->bit_pos = 0;
		bs->byte_pos++;
	}
}

/**
 * Returns the current byte position (for calculating offsets).
 */
static uint32_t get_byte_position(bitstream_writer_t *bs)
{
	return bs->byte_pos + (bs->bit_pos > 0 ? 1 : 0);
}

/**
 * Writes raw bytes to the bitstream (must be byte-aligned).
 */
static void write_bytes(bitstream_writer_t *bs, const uint8_t *data, uint32_t length)
{
	align_to_byte(bs);
	if (bs->byte_pos + length > bs->max_size)
	{
		if (!bs->error)
			fprintf(stderr, "pack: output buffer overflow\n");
		bs->error = 1;
		return;
	}
	memcpy(bs->data + bs->byte_pos, data, length);
	bs->byte_pos += length;
}

/* ============================================================================
 * Symbol Frequency Counter and Huffman Tree Builder
 * ============================================================================ */

#define MAX_SYMBOLS 256

typedef struct
{
	uint32_t frequency[MAX_SYMBOLS];
	uint8_t code_length[MAX_SYMBOLS];
	uint8_t has_code[MAX_SYMBOLS];
	uint32_t code[MAX_SYMBOLS];
} huffman_table_t;

/**
 * Huffman tree node for building the tree.
 */
typedef struct huffman_node
{
	uint32_t freq;
	int16_t symbol;  // -1 for internal nodes
	struct huffman_node *left;
	struct huffman_node *right;
} huffman_node_t;

/**
 * Resets frequency counts for a Huffman table.
 */
static void reset_huffman_table(huffman_table_t *table)
{
	memset(table, 0, sizeof(huffman_table_t));
}

/**
 * Increments the frequency of a symbol.
 */
static void count_symbol(huffman_table_t *table, uint8_t symbol)
{
	table->frequency[symbol]++;
}

/**
 * Assigns Huffman codes by traversing the tree.
 */
static void assign_codes(huffman_node_t *node, huffman_table_t *table, uint32_t code, uint8_t depth)
{
	if (node == NULL) return;

	if (node->symbol >= 0)
	{
		// Leaf node
		table->code[node->symbol] = code;
		table->code_length[node->symbol] = depth;
		table->has_code[node->symbol] = 1;
		return;
	}

	// Internal node - recurse
	assign_codes(node->left, table, code, depth + 1);                    // Left = 0
	assign_codes(node->right, table, code | (1 << depth), depth + 1);    // Right = 1
}

/**
 * Builds the Huffman tree for a table's collected frequencies.
 *
 * Nodes are allocated from the caller's array, which must hold at least
 * MAX_SYMBOLS * 2 entries, because the returned tree points into it.
 *
 * Returns NULL when no symbol has a non-zero frequency. When exactly one does,
 * the root is that leaf - callers detect this with root->symbol >= 0.
 */
static huffman_node_t *build_tree(const huffman_table_t *table,
                                  huffman_node_t *nodes,
                                  int *out_symbol_count)
{
	huffman_node_t *queue[MAX_SYMBOLS * 2];
	int node_count = 0;
	int queue_size = 0;

	// Create leaf nodes for symbols with non-zero frequency
	for (int i = 0; i < MAX_SYMBOLS; i++)
	{
		if (table->frequency[i] > 0)
		{
			nodes[node_count].freq = table->frequency[i];
			nodes[node_count].symbol = i;
			nodes[node_count].left = NULL;
			nodes[node_count].right = NULL;
			queue[queue_size++] = &nodes[node_count];
			node_count++;
		}
	}

	*out_symbol_count = queue_size;

	if (queue_size == 0)
	{
		return NULL;
	}

	// Sort queue by frequency (simple bubble sort)
	for (int i = 0; i < queue_size - 1; i++)
	{
		for (int j = 0; j < queue_size - i - 1; j++)
		{
			if (queue[j]->freq > queue[j + 1]->freq)
			{
				huffman_node_t *temp = queue[j];
				queue[j] = queue[j + 1];
				queue[j + 1] = temp;
			}
		}
	}

	// Build tree by combining lowest frequency nodes
	while (queue_size > 1)
	{
		huffman_node_t *left = queue[0];
		huffman_node_t *right = queue[1];

		nodes[node_count].freq = left->freq + right->freq;
		nodes[node_count].symbol = -1;
		nodes[node_count].left = left;
		nodes[node_count].right = right;

		for (int i = 2; i < queue_size; i++)
		{
			queue[i - 2] = queue[i];
		}
		queue_size -= 2;

		// Insert new node in sorted position
		int insert_pos = queue_size;
		for (int i = 0; i < queue_size; i++)
		{
			if (nodes[node_count].freq < queue[i]->freq)
			{
				insert_pos = i;
				break;
			}
		}

		for (int i = queue_size; i > insert_pos; i--)
		{
			queue[i] = queue[i - 1];
		}
		queue[insert_pos] = &nodes[node_count];
		queue_size++;
		node_count++;
	}

	return queue[0];
}

/**
 * Builds Huffman codes from frequency counts.
 *
 * Contract: the caller must have zeroed `table` (e.g. via reset_huffman_table)
 * before calling. This does not clear has_code or code_length itself, so a
 * stale has_code would survive for any symbol whose frequency has since
 * dropped to zero.
 */
static void build_huffman_codes(huffman_table_t *table)
{
	huffman_node_t nodes[MAX_SYMBOLS * 2];
	int symbol_count = 0;

	huffman_node_t *root = build_tree(table, nodes, &symbol_count);

	if (root == NULL)
	{
		return;
	}

	if (root->symbol >= 0)
	{
		// A single-symbol table serializes to a bare terminal, which the
		// decoder resolves without consuming a bit - so the code that
		// matches it is 0 bits wide, not 1.
		table->code[root->symbol] = 0;
		table->code_length[root->symbol] = 0;
		table->has_code[root->symbol] = 1;
		return;
	}

	assign_codes(root, table, 0, 0);
}

/**
 * Writes a Huffman code for a symbol to the bitstream.
 */
static void write_huffman_symbol(bitstream_writer_t *bs, huffman_table_t *table, uint8_t symbol)
{
	if (!table->has_code[symbol])
	{
		if (!bs->error)
			fprintf(stderr, "pack: symbol %d has no Huffman code\n", symbol);
		bs->error = 1;
		return;
	}
	write_bits(bs, table->code[symbol], table->code_length[symbol]);
}

/**
 * Serializes a Huffman table to the skip-list format used by the decompressor.
 *
 * Format:
 *   - Each node has a skip count byte (or 2 bytes if >= 128)
 *   - Zero byte indicates a terminal (symbol follows)
 *   - Navigation: bit=0 skip forward, bit=1 continue to next
 */
typedef struct
{
	uint8_t data[4096];
	uint32_t size;
} serialized_table_t;

static void serialize_node(huffman_node_t *node, serialized_table_t *out);

static uint32_t calculate_subtree_size(huffman_node_t *node)
{
	if (node == NULL) return 0;
	if (node->symbol >= 0) return 2;  // Terminal: 0 byte + symbol byte

	uint32_t left_size = calculate_subtree_size(node->left);
	uint32_t right_size = calculate_subtree_size(node->right);

	// Skip count size (1 or 2 bytes) + subtrees
	uint32_t skip_size = (right_size >= 128) ? 2 : 1;
	return skip_size + left_size + right_size;
}

static void serialize_node(huffman_node_t *node, serialized_table_t *out)
{
	if (node == NULL) return;

	if (node->symbol >= 0)
	{
		// Terminal node: write 0 followed by symbol
		out->data[out->size++] = 0;
		out->data[out->size++] = node->symbol;
		return;
	}

	// Internal node: write skip count for right subtree
	// When bit=0, we skip to right subtree; when bit=1, we continue to left
	// Wait - the decoder does: bit=0 skip forward (left branch), bit=1 next entry (right branch)
	// So right subtree comes first (next entry), left subtree is skipped to

	uint32_t right_size = calculate_subtree_size(node->right);

	if (right_size >= 128)
	{
		// Extended format: negative byte + low byte
		out->data[out->size++] = 0x80 | ((right_size >> 8) & 0x7F);
		out->data[out->size++] = right_size & 0xFF;
	}
	else
	{
		out->data[out->size++] = right_size;
	}

	// Write right subtree first (bit=1 path, next entry)
	serialize_node(node->right, out);

	// Write left subtree (bit=0 path, after skip)
	serialize_node(node->left, out);
}

/**
 * Builds and serializes a Huffman table from collected frequencies.
 * Returns the serialized table data.
 */
static void build_and_serialize_huffman(huffman_table_t *table, serialized_table_t *out)
{
	huffman_node_t nodes[MAX_SYMBOLS * 2];
	int symbol_count = 0;

	out->size = 0;

	huffman_node_t *root = build_tree(table, nodes, &symbol_count);

	if (root == NULL)
	{
		// Empty table - write single terminal with symbol 0
		out->data[out->size++] = 0;
		out->data[out->size++] = 0;
		return;
	}

	if (root->symbol >= 0)
	{
		// Single symbol - a bare terminal, read back with no bit consumed.
		// Paired with the single-symbol branch in build_huffman_codes: both
		// encode that a lone symbol's code is 0 bits wide, so don't change
		// one without checking the other.
		out->data[out->size++] = 0;
		out->data[out->size++] = root->symbol;
		return;
	}

	serialize_node(root, out);
}

/* ============================================================================
 * Move-to-End (MTE) Encoder
 *
 * The `mtf` prefix on the identifiers below is historical. This is NOT the
 * Move-to-Front of the compression literature: the KI algorithm moves the
 * symbol to index 31, so recently used registers get the HIGH indices. See
 * "Why Move-to-End?" in README.md, and mtf_move_to_end() in unpack.c, which is
 * the matching decoder.
 * ============================================================================ */

typedef struct
{
	uint8_t table[32];
} mtf_table_t;

/**
 * Initializes an MTE table with values [31, 30, ..., 1, 0].
 */
static void init_mtf_table(mtf_table_t *mtf)
{
	for (int i = 0; i < 32; i++)
	{
		mtf->table[i] = 31 - i;
	}
}

/**
 * Encodes a value using MTE and returns the index.
 * Also updates the table by moving the value to end (matching unpack.c behavior).
 */
static uint8_t mtf_encode(mtf_table_t *mtf, uint8_t value)
{
	// Find the value in the table
	uint8_t index = 0;
	for (int i = 0; i < 32; i++)
	{
		if (mtf->table[i] == value)
		{
			index = i;
			break;
		}
	}

	// Move to end (shift elements left, place value at position 31)
	for (int i = index; i < 31; i++)
	{
		mtf->table[i] = mtf->table[i + 1];
	}
	mtf->table[31] = value;

	return index;
}

/* ============================================================================
 * MIPS Instruction Analyzer
 * ============================================================================ */

/**
 * Extracts fields from a 32-bit MIPS instruction.
 */
typedef struct
{
	uint8_t opcode;      // Bits 26-31
	uint8_t rs;          // Bits 21-25
	uint8_t rt;          // Bits 16-20
	uint8_t rd;          // Bits 11-15
	uint8_t shamt;       // Bits 6-10
	uint8_t funct;       // Bits 0-5
	uint16_t immediate;  // Bits 0-15 (I-type)
	uint32_t target;     // Bits 0-25 (J-type)
} mips_instruction_t;

static void decode_mips_instruction(uint32_t instruction, mips_instruction_t *decoded)
{
	decoded->opcode = (instruction >> 26) & 0x3F;
	decoded->rs = (instruction >> 21) & 0x1F;
	decoded->rt = (instruction >> 16) & 0x1F;
	decoded->rd = (instruction >> 11) & 0x1F;
	decoded->shamt = (instruction >> 6) & 0x1F;
	decoded->funct = instruction & 0x3F;
	decoded->immediate = instruction & 0xFFFF;
	decoded->target = instruction & 0x03FFFFFF;
}

/* ============================================================================
 * Format Flags Lookup Tables (from ROM)
 * ============================================================================ */

// These tables define how each instruction is encoded
// Format flags bits:
//   0x0001: rs uses MTF table 1
//   0x0002: rs direct encoding
//   0x0004: rt uses MTF table 1
//   0x0008: rt direct encoding
//   0x0010: rd uses MTF table 1
//   0x0020: rd direct encoding
//   0x0040: shamt uses specialized table
//   0x0080: shamt direct encoding
//   0x0100: 26-bit jump target (split into 4 parts)
//   0x0200: rs uses MTF table 2
//   0x0400: immediate type 1
//   0x0800: immediate type 2
//   0x0C00: immediate type 3
//   0x1000: immediate direct (low byte only)
//   0x2000: NOP instruction
//   0x4000: JR $ra instruction
//   0xFFFF: Extended format

// Opcode table (64 entries) - extracted from KI1 ROM at 0x0f48 (little-endian)
static const uint16_t opcode_table[64] = {
	0x0000,  //  0: SPECIAL  - uses funct table
	0x0000,  //  1: REGIMM   - uses regimm table
	0x0100,  //  2: J        - 26-bit target split into 4 parts
	0x0100,  //  3: JAL      - 26-bit target split into 4 parts
	0x0c05,  //  4: BEQ      - rt:MTF1, rs:MTF1, imm16:type3
	0x0c05,  //  5: BNE      - rt:MTF1, rs:MTF1, imm16:type3
	0x0c09,  //  6: BLEZ     - rt:direct, rs:MTF1, imm16:type3
	0x0c09,  //  7: BGTZ     - rt:direct, rs:MTF1, imm16:type3
	0x0805,  //  8: ADDI     - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  //  9: ADDIU    - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 10: SLTI     - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 11: SLTIU    - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 12: ANDI     - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 13: ORI      - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 14: XORI     - rt:MTF1, rs:MTF1, imm16:type2
	0x0806,  // 15: LUI      - rs:direct, rt:MTF1, imm16:type2
	0xffff,  // 16: COP0     - extended format
	0xffff,  // 17: COP1     - extended format
	0xffff,  // 18: COP2     - extended format
	0x0000,  // 19: COP3     - raw/special
	0x0c05,  // 20: BEQL     - rt:MTF1, rs:MTF1, imm16:type3
	0x0c05,  // 21: BNEL     - rt:MTF1, rs:MTF1, imm16:type3
	0x0c09,  // 22: BLEZL    - rt:direct, rs:MTF1, imm16:type3
	0x0c09,  // 23: BGTZL    - rt:direct, rs:MTF1, imm16:type3
	0x0805,  // 24: DADDI    - rt:MTF1, rs:MTF1, imm16:type2
	0x0805,  // 25: DADDIU   - rt:MTF1, rs:MTF1, imm16:type2
	0x0605,  // 26: LDL      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 27: LDR      - rt:MTF1, rs:MTF2, imm16:type1
	0x0000,  // 28: reserved
	0x0000,  // 29: reserved
	0x0000,  // 30: reserved
	0x0000,  // 31: reserved
	0x0605,  // 32: LB       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 33: LH       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 34: LWL      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 35: LW       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 36: LBU      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 37: LHU      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 38: LWR      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 39: LWU      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 40: SB       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 41: SH       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 42: SWL      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 43: SW       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 44: SDL      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 45: SDR      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 46: SWR      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 47: CACHE    - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 48: LL       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 49: LWC1     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 50: LWC2     - rt:MTF1, rs:MTF2, imm16:type1
	0x0000,  // 51: PREF     - raw/special
	0x0605,  // 52: LLD      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 53: LDC1     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 54: LDC2     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 55: LD       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 56: SC       - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 57: SWC1     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 58: SWC2     - rt:MTF1, rs:MTF2, imm16:type1
	0x0000,  // 59: reserved
	0x0605,  // 60: SCD      - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 61: SDC1     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 62: SDC2     - rt:MTF1, rs:MTF2, imm16:type1
	0x0605,  // 63: SD       - rt:MTF1, rs:MTF2, imm16:type1
};

// Function code table (64 entries, for opcode=0 SPECIAL) - extracted from KI1 ROM at 0x0e48 (little-endian)
static const uint16_t funct_table[64] = {
	0x0056,  //  0: SLL      - rt:MTF1, rd:MTF1, sa:huff
	0x0000,  //  1: reserved
	0x0056,  //  2: SRL      - rt:MTF1, rd:MTF1, sa:huff
	0x0056,  //  3: SRA      - rt:MTF1, rd:MTF1, sa:huff
	0x0095,  //  4: SLLV     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0000,  //  5: reserved
	0x0095,  //  6: SRLV     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  //  7: SRAV     - rs:MTF1, rt:MTF1, rd:MTF1
	0x00a9,  //  8: JR       - rs:MTF1
	0x0099,  //  9: JALR     - rs:MTF1, rd:MTF1
	0x0000,  // 10: MOVZ
	0x0000,  // 11: MOVN
	0x0000,  // 12: SYSCALL  - raw
	0x0000,  // 13: BREAK    - raw
	0x0000,  // 14: reserved
	0x0000,  // 15: SYNC
	0x009a,  // 16: MFHI     - rd:MTF1
	0x00a9,  // 17: MTHI     - rs:MTF1
	0x009a,  // 18: MFLO     - rd:MTF1
	0x00a9,  // 19: MTLO     - rs:MTF1
	0x0095,  // 20: DSLLV    - rs:MTF1, rt:MTF1, rd:MTF1
	0x0000,  // 21: reserved
	0x0095,  // 22: DSRLV    - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 23: DSRAV    - rs:MTF1, rt:MTF1, rd:MTF1
	0x00a5,  // 24: MULT     - rs:MTF1, rt:MTF1
	0x00a5,  // 25: MULTU    - rs:MTF1, rt:MTF1
	0x00a5,  // 26: DIV      - rs:MTF1, rt:MTF1
	0x00a5,  // 27: DIVU     - rs:MTF1, rt:MTF1
	0x00a5,  // 28: DMULT    - rs:MTF1, rt:MTF1
	0x00a5,  // 29: DMULTU   - rs:MTF1, rt:MTF1
	0x00a5,  // 30: DDIV     - rs:MTF1, rt:MTF1
	0x00a5,  // 31: DDIVU    - rs:MTF1, rt:MTF1
	0x0095,  // 32: ADD      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 33: ADDU     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 34: SUB      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 35: SUBU     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 36: AND      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 37: OR       - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 38: XOR      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 39: NOR      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0000,  // 40: reserved
	0x0000,  // 41: reserved
	0x0095,  // 42: SLT      - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 43: SLTU     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 44: DADD     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 45: DADDU    - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 46: DSUB     - rs:MTF1, rt:MTF1, rd:MTF1
	0x0095,  // 47: DSUBU    - rs:MTF1, rt:MTF1, rd:MTF1
	0x00a9,  // 48: TGE      - rs:MTF1, rt:MTF1
	0x00a9,  // 49: TGEU     - rs:MTF1, rt:MTF1
	0x00a9,  // 50: TLT      - rs:MTF1, rt:MTF1
	0x00a9,  // 51: TLTU     - rs:MTF1, rt:MTF1
	0x00a9,  // 52: TEQ      - rs:MTF1, rt:MTF1
	0x0000,  // 53: reserved
	0x00a9,  // 54: TNE      - rs:MTF1, rt:MTF1
	0x0000,  // 55: reserved
	0x0056,  // 56: DSLL     - rt:MTF1, rd:MTF1, sa:huff
	0x0000,  // 57: reserved
	0x0056,  // 58: DSRL     - rt:MTF1, rd:MTF1, sa:huff
	0x0056,  // 59: DSRA     - rt:MTF1, rd:MTF1, sa:huff
	0x0056,  // 60: DSLL32   - rt:MTF1, rd:MTF1, sa:huff
	0x0000,  // 61: reserved
	0x0056,  // 62: DSRL32   - rt:MTF1, rd:MTF1, sa:huff
	0x0056,  // 63: DSRA32   - rt:MTF1, rd:MTF1, sa:huff
};

// REGIMM table (32 entries, for opcode=1) - extracted from KI1 ROM at 0x0ec8 (little-endian)
static const uint16_t regimm_table[32] = {
	0x0c01,  //  0: BLTZ     - rs:MTF1, imm16:t3
	0x0c01,  //  1: BGEZ     - rs:MTF1, imm16:t3
	0x0c01,  //  2: BLTZL    - rs:MTF1, imm16:t3
	0x0c01,  //  3: BGEZL    - rs:MTF1, imm16:t3
	0x0000,  //  4: reserved
	0x0000,  //  5: reserved
	0x0000,  //  6: reserved
	0x0000,  //  7: reserved
	0x0801,  //  8: TGEI     - rs:MTF1, imm16:t2
	0x0801,  //  9: TGEIU    - rs:MTF1, imm16:t2
	0x0801,  // 10: TLTI     - rs:MTF1, imm16:t2
	0x0801,  // 11: TLTIU    - rs:MTF1, imm16:t2
	0x0801,  // 12: TEQI     - rs:MTF1, imm16:t2
	0x0000,  // 13: reserved
	0x0801,  // 14: TNEI     - rs:MTF1, imm16:t2
	0x0000,  // 15: reserved
	0x0c01,  // 16: BLTZAL   - rs:MTF1, imm16:t3
	0x0c01,  // 17: BGEZAL   - rs:MTF1, imm16:t3
	0x0c01,  // 18: BLTZALL  - rs:MTF1, imm16:t3
	0x0c01,  // 19: BGEZALL  - rs:MTF1, imm16:t3
	0x0000,  // 20: reserved
	0x0000,  // 21: reserved
	0x0000,  // 22: reserved
	0x0000,  // 23: reserved
	0x0000,  // 24: reserved
	0x0000,  // 25: reserved
	0x0000,  // 26: reserved
	0x0000,  // 27: reserved
	0x0000,  // 28: reserved
	0x0000,  // 29: reserved
	0x0000,  // 30: reserved
	0x0000,  // 31: reserved
};

// Extended table (32 entries, for COP0/COP1/COP2 rs field) - extracted from KI1 ROM at 0x0f08 (little-endian)
static const uint16_t extended_table[32] = {
	0x1094,  //  0: MFC      - rt:MTF1, rd:direct
	0x1094,  //  1: DMFC     - rt:MTF1, rd:direct
	0x1094,  //  2: CFC      - rt:MTF1, rd:direct
	0x0000,  //  3: reserved
	0x1094,  //  4: MTC      - rt:MTF1, rd:direct
	0x1094,  //  5: DMTC     - rt:MTF1, rd:direct
	0x1094,  //  6: CTC      - rt:MTF1, rd:direct
	0x0000,  //  7: reserved
	0x0c00,  //  8: BC       - imm16:t3 (coprocessor branch)
	0x0000,  //  9: reserved
	0x0000,  // 10: reserved
	0x0000,  // 11: reserved
	0x0000,  // 12: reserved
	0x0000,  // 13: reserved
	0x0000,  // 14: reserved
	0x0000,  // 15: reserved
	0xffff,  // 16: CO       - raw 21-bit
	0xffff,  // 17: CO       - raw 21-bit
	0xffff,  // 18: CO       - raw 21-bit
	0xffff,  // 19: CO       - raw 21-bit
	0xffff,  // 20: CO       - raw 21-bit
	0xffff,  // 21: CO       - raw 21-bit
	0xffff,  // 22: CO       - raw 21-bit
	0xffff,  // 23: CO       - raw 21-bit
	0xffff,  // 24: CO       - raw 21-bit
	0xffff,  // 25: CO       - raw 21-bit
	0xffff,  // 26: CO       - raw 21-bit
	0xffff,  // 27: CO       - raw 21-bit
	0xffff,  // 28: CO       - raw 21-bit
	0xffff,  // 29: CO       - raw 21-bit
	0xffff,  // 30: CO       - raw 21-bit
	0xffff,  // 31: CO       - raw 21-bit
};

/* ============================================================================
 * Huffman Tables (24 tables used during compression)
 * ============================================================================ */

// Table indices (workspace offsets)
#define TABLE_OPCODE        0   // 0x40
#define TABLE_FUNCT         1   // 0x44
#define TABLE_REGIMM        2   // 0x48
#define TABLE_RS_MTF1       3   // 0x4C
#define TABLE_RT_MTF1       4   // 0x50
#define TABLE_RD_MTF1       5   // 0x54
#define TABLE_SHAMT         6   // 0x58
#define TABLE_RS_MTF2       7   // 0x5C
#define TABLE_DIRECT        8   // 0x60
#define TABLE_IMM_HI_1      9   // 0x64
#define TABLE_IMM_LO_1      10  // 0x68
#define TABLE_JUMP_0        11  // 0x6C
#define TABLE_JUMP_1        12  // 0x70
#define TABLE_JUMP_2        13  // 0x74
#define TABLE_JUMP_3        14  // 0x78
#define TABLE_IMM_HI_2      15  // 0x7C
#define TABLE_IMM_LO_2      16  // 0x80
#define TABLE_IMM_HI_3      17  // 0x84
#define TABLE_IMM_LO_3      18  // 0x88
#define TABLE_IMM_HI_4      19  // 0x8C
#define TABLE_IMM_LO_4      20  // 0x90
#define TABLE_RS_DIRECT     21  // 0x94
#define TABLE_EXTENDED      22  // 0x98
#define TABLE_COP_RT        23  // 0x9C

/* ============================================================================
 * File Loading
 * ============================================================================ */

typedef struct
{
	uint8_t *data;
	uint32_t size;
	uint32_t load_addr;
} input_file_t;

/**
 * Loads a .bin file and its corresponding .addr file.
 */
static int load_input_file(const char *input_dir, int file_id, input_file_t *file)
{
	char bin_path[512];
	char addr_path[512];

	snprintf(bin_path, sizeof(bin_path), "%s/rom-%d.bin", input_dir, file_id);
	snprintf(addr_path, sizeof(addr_path), "%s/rom-%d.addr", input_dir, file_id);

	// Load address file
	FILE *addr_fd = fopen(addr_path, "rb");
	if (addr_fd == NULL)
	{
		return 0;  // File doesn't exist
	}
	// Unchecked, a short or empty .addr leaves load_addr holding whatever was
	// on the stack, and that value is written into the archive as the
	// segment's load address.
	if (fread(&file->load_addr, 4, 1, addr_fd) != 1)
	{
		fprintf(stderr, "pack: %s: short read, expected 4 bytes\n", addr_path);
		fclose(addr_fd);
		return -1;
	}
	fclose(addr_fd);

	// Load binary file
	FILE *bin_fd = fopen(bin_path, "rb");
	if (bin_fd == NULL)
	{
		fprintf(stderr, "pack: %s: cannot open\n", bin_path);
		return -1;
	}

	fseek(bin_fd, 0, SEEK_END);
	file->size = ftell(bin_fd);
	fseek(bin_fd, 0, SEEK_SET);

	// The stream header carries file->size verbatim, but only size/4
	// instructions are ever encoded. unpack counts down from that header by 4
	// and stops at exactly 0, so a size that is not a multiple of 4 wraps its
	// unsigned counter and spins forever. Refuse it here instead.
	if (file->size % 4 != 0)
	{
		fprintf(stderr, "pack: %s: size %u is not a multiple of 4 "
		        "(input must be whole 32-bit MIPS instructions)\n",
		        bin_path, file->size);
		fclose(bin_fd);
		return -1;
	}

	// A zero-length segment is legal - pack writes its header and skips the
	// body - but malloc(0) is allowed to return NULL, which would look like an
	// allocation failure here. Ask for one byte instead.
	file->data = malloc(file->size != 0 ? file->size : 1);
	if (file->data == NULL)
	{
		fprintf(stderr, "pack: %s: cannot allocate %u bytes\n", bin_path, file->size);
		fclose(bin_fd);
		return -1;
	}

	if (fread(file->data, 1, file->size, bin_fd) != (size_t)file->size)
	{
		fprintf(stderr, "pack: %s: short read, expected %u bytes\n", bin_path, file->size);
		free(file->data);
		file->data = NULL;
		fclose(bin_fd);
		return -1;
	}
	fclose(bin_fd);

	printf("Loaded rom-%d: size=0x%x, addr=0x%08x\n", file_id, file->size, file->load_addr);
	return 1;
}

/* ============================================================================
 * Main Compression Functions
 * ============================================================================ */

/**
 * First pass: collect symbol frequencies for all Huffman tables.
 */
static void collect_frequencies(input_file_t *files, int file_count, huffman_table_t tables[KIPACK_NUM_TABLES])
{
	// Reset all tables
	for (int t = 0; t < KIPACK_NUM_TABLES; t++)
	{
		reset_huffman_table(&tables[t]);
	}

	for (int f = 0; f < file_count; f++)
	{
		input_file_t *file = &files[f];
		uint32_t *instructions = (uint32_t *)file->data;
		uint32_t instr_count = file->size / 4;

		mtf_table_t mtf1, mtf2;
		init_mtf_table(&mtf1);
		init_mtf_table(&mtf2);

		for (uint32_t i = 0; i < instr_count; i++)
		{
			uint32_t instr = instructions[i];
			mips_instruction_t decoded;
			decode_mips_instruction(instr, &decoded);

			// Count opcode
			count_symbol(&tables[TABLE_OPCODE], decoded.opcode);

			uint16_t format_flags = opcode_table[decoded.opcode];

			if (decoded.opcode == 0)
			{
				// SPECIAL: count funct
				count_symbol(&tables[TABLE_FUNCT], decoded.funct);
				format_flags = funct_table[decoded.funct];

				// NOP (instr == 0) is SLL $zero, $zero, 0 - needs full field frequency counting
				// (funct_table[0] = 0x0056 means rt:MTF1, rs:direct, rd:MTF1, sa:huff)

				// Raw 20-bit encoding for reserved SPECIAL instructions
				if (format_flags == 0)
				{
					continue;  // No Huffman symbols, just raw bits
				}
			}
			else if (decoded.opcode == 1)
			{
				// REGIMM: count rt as branch type
				count_symbol(&tables[TABLE_REGIMM], decoded.rt);
				format_flags = regimm_table[decoded.rt];

				if (format_flags == 0)
				{
					// Reserved REGIMM - count fallback encoding
					count_symbol(&tables[TABLE_RS_DIRECT], decoded.rs);
					count_symbol(&tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
					count_symbol(&tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
					continue;  // Skip field-based counting
				}
			}
			else if (format_flags == 0xFFFF)
			{
				// Extended format (COP0/COP1/COP2)
				count_symbol(&tables[TABLE_EXTENDED], decoded.rs);
				format_flags = extended_table[decoded.rs];

				if (format_flags == 0)
				{
					// Reserved extended - count fallback encoding
					count_symbol(&tables[TABLE_RS_DIRECT], decoded.rt);
					count_symbol(&tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
					count_symbol(&tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
					continue;  // Skip field-based counting
				}
				else if (format_flags == 0xC00)
				{
					count_symbol(&tables[TABLE_COP_RT], decoded.rt);
					if (decoded.rt >= 4)
					{
						count_symbol(&tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
						count_symbol(&tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
						continue;  // Extended immediate handled
					}
					// For rt<4, fall through to count immediate with type 3 tables (0xC00)
				}
				else if (format_flags == 0xFFFF)
				{
					// Raw 21-bit - no symbols to count
					continue;
				}
			}
			else if (format_flags == 0)
			{
				// Raw 26-bit jump target (reserved opcode)
				continue;
			}
			else if (format_flags == 0x4000 || format_flags == 0x2000)
			{
				// JR $ra or NOP - fully encoded by opcode.
				//
				// DEAD, AND PERMANENTLY SO. format_flags here only ever comes
				// from opcode_table/funct_table/regimm_table/extended_table
				// above, which in pack are static const arrays - no entry holds
				// 0x4000 or 0x2000, and none can at runtime. Kept only to
				// mirror unpack's dispatch.
				//
				// Note the asymmetry with unpack.c: the same two branches there
				// switch on tables read out of the ROM at runtime, so they are
				// dead for the shipped tables only and MUST be kept. Do not
				// carry a deletion here across to unpack.
				continue;
			}

			// Count fields based on format flags
			if (format_flags & 0x4)
			{
				uint8_t mtf_index = mtf_encode(&mtf1, decoded.rt);
				count_symbol(&tables[TABLE_RT_MTF1], mtf_index);
			}
			else if (format_flags & 0x8)
			{
				count_symbol(&tables[TABLE_DIRECT], decoded.rt);
			}

			if (format_flags & 0x1)
			{
				if (format_flags & 0x200)
				{
					uint8_t mtf_index = mtf_encode(&mtf2, decoded.rs);
					count_symbol(&tables[TABLE_RS_MTF2], mtf_index);
				}
				else
				{
					uint8_t mtf_index = mtf_encode(&mtf1, decoded.rs);
					count_symbol(&tables[TABLE_RS_MTF1], mtf_index);
				}
			}
			else if (format_flags & 0x2)
			{
				count_symbol(&tables[TABLE_DIRECT], decoded.rs);
			}

			if (format_flags & 0x10)
			{
				uint8_t mtf_index = mtf_encode(&mtf1, decoded.rd);
				count_symbol(&tables[TABLE_RD_MTF1], mtf_index);
			}
			else if (format_flags & 0x20)
			{
				count_symbol(&tables[TABLE_DIRECT], decoded.rd);
			}

			if (format_flags & 0x40)
			{
				count_symbol(&tables[TABLE_SHAMT], decoded.shamt);
			}
			else if (format_flags & 0x80)
			{
				count_symbol(&tables[TABLE_DIRECT], decoded.shamt);
			}

			if (format_flags & 0x100)
			{
				// Jump target split into 4 parts
				count_symbol(&tables[TABLE_JUMP_0], (decoded.target >> 20) & 0x3F);
				count_symbol(&tables[TABLE_JUMP_1], (decoded.target >> 14) & 0x3F);
				count_symbol(&tables[TABLE_JUMP_2], (decoded.target >> 7) & 0x7F);
				count_symbol(&tables[TABLE_JUMP_3], decoded.target & 0x7F);
			}

			uint16_t imm_type = format_flags & 0xC00;
			if (imm_type == 0x400)
			{
				count_symbol(&tables[TABLE_IMM_HI_2], (decoded.immediate >> 8) & 0xFF);
				count_symbol(&tables[TABLE_IMM_LO_2], decoded.immediate & 0xFF);
			}
			else if (imm_type == 0x800)
			{
				count_symbol(&tables[TABLE_IMM_HI_3], (decoded.immediate >> 8) & 0xFF);
				count_symbol(&tables[TABLE_IMM_LO_3], decoded.immediate & 0xFF);
			}
			else if (imm_type == 0xC00)
			{
				count_symbol(&tables[TABLE_IMM_HI_4], (decoded.immediate >> 8) & 0xFF);
				count_symbol(&tables[TABLE_IMM_LO_4], decoded.immediate & 0xFF);
			}
			else if (format_flags & 0x1000)
			{
				count_symbol(&tables[TABLE_DIRECT], decoded.immediate & 0xFF);
			}
		}
	}

	// Build Huffman codes for all tables
	for (int t = 0; t < KIPACK_NUM_TABLES; t++)
	{
		build_huffman_codes(&tables[t]);
	}
}

/**
 * Second pass: encode instructions using the built Huffman tables.
 */
static void encode_file(input_file_t *file, huffman_table_t tables[KIPACK_NUM_TABLES], bitstream_writer_t *bs)
{
	uint32_t *instructions = (uint32_t *)file->data;
	uint32_t instr_count = file->size / 4;

	mtf_table_t mtf1, mtf2;
	init_mtf_table(&mtf1);
	init_mtf_table(&mtf2);

	for (uint32_t i = 0; i < instr_count; i++)
	{
		uint32_t instr = instructions[i];
		mips_instruction_t decoded;
		decode_mips_instruction(instr, &decoded);

		// Write opcode
		write_huffman_symbol(bs, &tables[TABLE_OPCODE], decoded.opcode);

		uint16_t format_flags = opcode_table[decoded.opcode];

		if (decoded.opcode == 0)
		{
			// SPECIAL instructions
			write_huffman_symbol(bs, &tables[TABLE_FUNCT], decoded.funct);
			format_flags = funct_table[decoded.funct];

			// NOP (instr == 0) is SLL $zero, $zero, 0 - needs full field encoding
			// (funct_table[0] = 0x0056 means rt:MTF1, rs:direct, rd:MTF1, sa:huff)

			if (format_flags == 0)
			{
				// Raw 20-bit value
				write_bits(bs, (instr >> 6) & 0xFFFFF, 20);
				continue;
			}
		}
		else if (decoded.opcode == 1)
		{
			// REGIMM instructions
			write_huffman_symbol(bs, &tables[TABLE_REGIMM], decoded.rt);
			format_flags = regimm_table[decoded.rt];
	
			if (format_flags == 0)
			{
				// Full decode needed
				write_huffman_symbol(bs, &tables[TABLE_RS_DIRECT], decoded.rs);
				write_huffman_symbol(bs, &tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
				write_huffman_symbol(bs, &tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
				continue;
			}
		}
		else if (format_flags == 0xFFFF)
		{
			// Extended format
			write_huffman_symbol(bs, &tables[TABLE_EXTENDED], decoded.rs);
			format_flags = extended_table[decoded.rs];
	
			if (format_flags == 0)
			{
				write_huffman_symbol(bs, &tables[TABLE_RS_DIRECT], decoded.rt);
				write_huffman_symbol(bs, &tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
				write_huffman_symbol(bs, &tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
				continue;
			}
			else if (format_flags == 0xC00)
			{
				write_huffman_symbol(bs, &tables[TABLE_COP_RT], decoded.rt);
				// For rt>=4, immediate is decoded in extended format block
				// For rt<4, immediate is decoded later in the "immediate type 3" block
				// Either way, don't 'continue' - let the normal field encoding handle the immediate
				if (decoded.rt >= 4)
				{
					write_huffman_symbol(bs, &tables[TABLE_IMM_HI_1], (decoded.immediate >> 8) & 0xFF);
					write_huffman_symbol(bs, &tables[TABLE_IMM_LO_1], decoded.immediate & 0xFF);
					continue;  // Extended immediate handled, skip normal field encoding
				}
				// For rt<4, fall through to normal format_flags processing (0xC00 = imm type 3)
			}
			else if (format_flags == 0xFFFF)
			{
				// Raw 21-bit value
				write_bits(bs, instr & 0x1FFFFF, 21);
				continue;
			}
		}
		else if (format_flags == 0)
		{
			// Raw 26-bit jump target
			write_bits(bs, decoded.target, 26);
			continue;
		}
		else if (format_flags == 0x4000)
		{
			// JR $ra - already encoded.
			// Dead for the same reason as the twin in collect_frequencies:
			// pack's four lookup tables are compile-time constants and no
			// entry holds 0x4000. See the longer note there before deleting
			// this - unpack's matching branch is NOT dead in the same sense.
			continue;
		}
		else if (format_flags == 0x2000)
		{
			// NOP - already encoded. Dead here for the same reason as 0x4000
			// above; see the note in collect_frequencies.
			continue;
		}

		// Encode fields based on format flags

		// rt field
		if (format_flags & 0x4)
		{
			uint8_t mtf_index = mtf_encode(&mtf1, decoded.rt);
			write_huffman_symbol(bs, &tables[TABLE_RT_MTF1], mtf_index);
		}
		else if (format_flags & 0x8)
		{
			write_huffman_symbol(bs, &tables[TABLE_DIRECT], decoded.rt);
		}

		// rs field
		if (format_flags & 0x1)
		{
			if (format_flags & 0x200)
			{
				uint8_t mtf_index = mtf_encode(&mtf2, decoded.rs);
				write_huffman_symbol(bs, &tables[TABLE_RS_MTF2], mtf_index);
			}
			else
			{
				uint8_t mtf_index = mtf_encode(&mtf1, decoded.rs);
				write_huffman_symbol(bs, &tables[TABLE_RS_MTF1], mtf_index);
			}
		}
		else if (format_flags & 0x2)
		{
			write_huffman_symbol(bs, &tables[TABLE_DIRECT], decoded.rs);
		}

		// rd field
		if (format_flags & 0x10)
		{
			uint8_t mtf_index = mtf_encode(&mtf1, decoded.rd);
			write_huffman_symbol(bs, &tables[TABLE_RD_MTF1], mtf_index);
		}
		else if (format_flags & 0x20)
		{
			write_huffman_symbol(bs, &tables[TABLE_DIRECT], decoded.rd);
		}

		// shamt field
		if (format_flags & 0x40)
		{
			write_huffman_symbol(bs, &tables[TABLE_SHAMT], decoded.shamt);
		}
		else if (format_flags & 0x80)
		{
			write_huffman_symbol(bs, &tables[TABLE_DIRECT], decoded.shamt);
		}

		// Jump target (26-bit split into 4 parts)
		if (format_flags & 0x100)
		{
			write_huffman_symbol(bs, &tables[TABLE_JUMP_0], (decoded.target >> 20) & 0x3F);
			write_huffman_symbol(bs, &tables[TABLE_JUMP_1], (decoded.target >> 14) & 0x3F);
			write_huffman_symbol(bs, &tables[TABLE_JUMP_2], (decoded.target >> 7) & 0x7F);
			write_huffman_symbol(bs, &tables[TABLE_JUMP_3], decoded.target & 0x7F);
		}

		// Immediate field
		uint16_t imm_type = format_flags & 0xC00;
		if (imm_type == 0x400)
		{
			write_huffman_symbol(bs, &tables[TABLE_IMM_HI_2], (decoded.immediate >> 8) & 0xFF);
			write_huffman_symbol(bs, &tables[TABLE_IMM_LO_2], decoded.immediate & 0xFF);
		}
		else if (imm_type == 0x800)
		{
			write_huffman_symbol(bs, &tables[TABLE_IMM_HI_3], (decoded.immediate >> 8) & 0xFF);
			write_huffman_symbol(bs, &tables[TABLE_IMM_LO_3], decoded.immediate & 0xFF);
		}
		else if (imm_type == 0xC00)
		{
			write_huffman_symbol(bs, &tables[TABLE_IMM_HI_4], (decoded.immediate >> 8) & 0xFF);
			write_huffman_symbol(bs, &tables[TABLE_IMM_LO_4], decoded.immediate & 0xFF);
		}
		else if (format_flags & 0x1000)
		{
			write_huffman_symbol(bs, &tables[TABLE_DIRECT], decoded.immediate & 0xFF);
		}
	}
}

/**
 * ROM layout offsets for pack.c output format.
 * This mimics the KI bootrom structure so unpack.c can read it directly.
 *
 * Layout:
 *   0x0000 - 0x007F: Function code table (64 entries × 2 bytes = 128 bytes)
 *   0x0080 - 0x00BF: REGIMM table (32 entries × 2 bytes = 64 bytes)
 *   0x00C0 - 0x00FF: Extended table (32 entries × 2 bytes = 64 bytes)
 *   0x0100 - 0x017F: Opcode table (64 entries × 2 bytes = 128 bytes)
 *   0x0180 - ...   : Compressed data stream
 */

/**
 * Main compression function.
 */
static int compress_files(input_file_t *files, int file_count, const char *output_path)
{
	huffman_table_t tables[KIPACK_NUM_TABLES];
	serialized_table_t serialized_tables[KIPACK_NUM_TABLES];

	printf("Collecting symbol frequencies...\n");
	collect_frequencies(files, file_count, tables);

	printf("Building Huffman tables...\n");
	for (int t = 0; t < KIPACK_NUM_TABLES; t++)
	{
		build_and_serialize_huffman(&tables[t], &serialized_tables[t]);
	}

	// Allocate output buffer
	uint8_t *output = malloc(MAX_OUTPUT_SIZE);
	if (output == NULL)
	{
		fprintf(stderr, "pack: cannot allocate the output buffer\n");
		return -1;
	}
	memset(output, 0, MAX_OUTPUT_SIZE);

	// Write lookup tables at fixed offsets (ROM-compatible layout)
	printf("Writing lookup tables...\n");

	// Funct table at offset 0x0000
	memcpy(output + KIPACK_PACKED_FUNCT, funct_table, sizeof(funct_table));

	// REGIMM table at offset 0x0080
	memcpy(output + KIPACK_PACKED_REGIMM, regimm_table, sizeof(regimm_table));

	// Extended table at offset 0x00C0
	memcpy(output + KIPACK_PACKED_EXTENDED, extended_table, sizeof(extended_table));

	// Opcode table at offset 0x0100
	memcpy(output + KIPACK_PACKED_OPCODE, opcode_table, sizeof(opcode_table));

	// Start bitstream after the lookup tables
	bitstream_writer_t bs;
	init_bitstream_writer(&bs, output + KIPACK_PACKED_DATA,
	                      MAX_OUTPUT_SIZE - KIPACK_PACKED_DATA);

	// Write header
	printf("Writing header...\n");
	write_bits(&bs, KIPACK_MAGIC, 16);  // Magic "br"
	write_bits(&bs, file_count, 8);     // File count

	// Process each file
	for (int f = 0; f < file_count; f++)
	{
		input_file_t *file = &files[f];

		printf("Compressing file %d...\n", f);

		// Write file header
		write_bits(&bs, 0, 8);                    // File type (0 = code)
		write_bits(&bs, file->size, 32);         // Decompressed size
		// Convert physical address to KSEG0 cached virtual address
		// (add 0x80000000 to make it compatible with unpack.c)
		uint32_t virtual_addr = file->load_addr | 0x80000000;
		write_bits(&bs, virtual_addr, 32);       // Load address (KSEG0)

		if (file->size == 0)
		{
			continue;
		}

		// Write Huffman tables (24 tables)
		for (int t = 0; t < KIPACK_NUM_TABLES; t++)
		{
			align_to_byte(&bs);

			// Write table size as 16-bit offset
			write_bits(&bs, serialized_tables[t].size, 16);

			// Write table data
			write_bytes(&bs, serialized_tables[t].data, serialized_tables[t].size);
		}

		// Encode instructions
		encode_file(file, tables, &bs);
	}

	if (bs.error)
	{
		free(output);
		return -1;
	}

	// Finalize and write output
	align_to_byte(&bs);
	uint32_t compressed_size = get_byte_position(&bs);
	uint32_t output_size = KIPACK_PACKED_DATA + compressed_size;

	printf("Writing output (%u bytes total, %u bytes compressed data)...\n",
	       output_size, compressed_size);

	FILE *out_fd = fopen(output_path, "wb");
	if (out_fd == NULL)
	{
		fprintf(stderr, "pack: %s: cannot create\n", output_path);
		free(output);
		return -1;
	}

	// Unchecked, a short write yields a truncated archive that unpack reads as
	// a valid but wrong ROM. fclose is checked too: that is where the flush
	// happens, so a full disk can surface there rather than at the fwrite.
	if (fwrite(output, 1, output_size, out_fd) != (size_t)output_size)
	{
		fprintf(stderr, "pack: %s: short write\n", output_path);
		fclose(out_fd);
		free(output);
		return -1;
	}
	if (fclose(out_fd) != 0)
	{
		fprintf(stderr, "pack: %s: write failed on close\n", output_path);
		free(output);
		return -1;
	}

	free(output);

	printf("Compression complete: %u bytes\n", output_size);
	return 0;
}

int kipack_pack(const char *in_dir, const char *out_path, int max_files)
{
	input_file_t files[MAX_FILES];
	int file_count = 0;

	if (max_files > MAX_FILES)
		max_files = MAX_FILES;

	for (int i = 0; i < max_files; i++)
	{
		int result = load_input_file(in_dir, i, &files[file_count]);
		if (result == 0)
			break;
		if (result < 0)
		{
			for (int j = 0; j < file_count; j++)
				free(files[j].data);
			return -1;
		}
		file_count++;
	}

	if (file_count == 0)
	{
		fprintf(stderr, "pack: no rom-N.bin/.addr pairs found in %s\n", in_dir);
		return -1;
	}

	printf("Found %d input files\n", file_count);

	int result = compress_files(files, file_count, out_path);

	for (int i = 0; i < file_count; i++)
		free(files[i].data);

	return result;
}

int kipack_pack_main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: pack <in_dir> <out.packed> [count]\n");
		return 1;
	}

	int max_files = (argc > 3) ? atoi(argv[3]) : MAX_FILES;

	return kipack_pack(argv[1], argv[2], max_files) == 0 ? 0 : 1;
}
