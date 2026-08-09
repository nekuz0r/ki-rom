/**
 * mkbadstream.c - build deliberately malformed .packed archives.
 *
 * Every archive the suite expects unpack to refuse is built here, so the one
 * thing they all depend on - where a field sits in the stream - is derived
 * once from the constants in kipack.h rather than restated as byte offsets in
 * the test script.
 *
 * Two of the five need more than a corrupt field. The bounds guards in
 * unpack.c cannot be reached by changing a byte of a good archive at all: the
 * ROM window is 512 KB and load_rom zero-fills it, so a stream a few kilobytes
 * long has no way to run off the end. Reaching the end takes a chain of table
 * offsets, each one landing exactly where the next is read from.
 *
 * Every byte the chosen mode does not write comes from the input archive.
 *
 * Usage: mkbadstream <in.packed> <magic|count|size|seek|walk> <out.packed>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../kipack.h"

#define ROM_WINDOW_SIZE 0x80000

/*
 * The stream's header, and where each field lands.
 *
 * It opens at KIPACK_PACKED_DATA with a 16-bit magic and an 8-bit file count,
 * then the first file's 8-bit type, 32-bit size and 32-bit destination: 96
 * bits exactly, so everything is byte-aligned and the first table's 16-bit
 * offset field lands 12 bytes in.
 */
#define HDR_MAGIC   (KIPACK_PACKED_DATA + 0)
#define HDR_COUNT   (KIPACK_PACKED_DATA + 2)
#define HDR_SIZE    (KIPACK_PACKED_DATA + 4)
#define FIRST_FIELD (KIPACK_PACKED_DATA + 12)

/* Break the "br" the stream opens with. Rejected outright, never looped on. */
static void make_magic(uint8_t *image)
{
	image[HDR_MAGIC] = 0xFF;
}

/*
 * A file count of zero. unpack's file loop decrements after each pass, so zero
 * underflows its unsigned counter and used to spin forever.
 */
static void make_count(uint8_t *image)
{
	image[HDR_COUNT] = 0x00;
}

/*
 * A decompressed size that is not a whole number of instructions. The
 * instruction loop counts down by 4 and stops at exactly 0, so a size that is
 * not a multiple of 4 wraps its counter and used to spin forever.
 *
 * Only the size field's low byte is touched, so this holds whatever the corpus
 * happens to be: 0x01 is odd, hence never a multiple of 4.
 */
static void make_size(uint8_t *image)
{
	image[HDR_SIZE] = 0x01;
}

/*
 * unpack's table loop as an address walk: it reads a 16-bit offset at `field`,
 * takes the two bytes following it as that table's base, and seeks to
 * base + offset to find the next field. So field(n+1) = field(n) + 2 + off(n).
 */
static void put16(uint8_t *image, size_t at, uint16_t value)
{
	image[at]     = (uint8_t)(value);
	image[at + 1] = (uint8_t)(value >> 8);
}

/*
 * Chain the offsets at the 16-bit maximum until a seek target leaves the
 * window. One field cannot do it - the first table's base is 0x18E and the
 * offset is 16 bits - so it takes eight hops.
 */
static void make_seek(uint8_t *image)
{
	size_t field = FIRST_FIELD;

	for (int i = 0; i < KIPACK_NUM_TABLES; i++)
	{
		put16(image, field, 0xFFFF);

		size_t base = field + 2;
		size_t target = base + 0xFFFF;

		// The next iteration's put16() writes two bytes at [target], so the
		// guard has to fail before target's second byte would leave the
		// calloc'd image, not just before target itself does.
		if (target + 2 > ROM_WINDOW_SIZE)
		{
			return;  /* this is the seek unpack has to refuse */
		}
		field = target;
	}

	fprintf(stderr, "mkbadstream: seek chain stayed inside the window\n");
	exit(1);
}

/* Keeps all 24 table headers packed into a known, in-window range. */
#define WALK_STRIDE 0x100

/* Largest hop one tree node can take: a 15-bit skip plus the node's 2 bytes. */
#define WALK_HOP (0x7FFF + 2)

/*
 * Keep every table inside the window, but hand table 0 a Huffman tree whose
 * skip counts march it off the end.
 *
 * A node byte with the high bit set means an extended skip: its low 7 bits and
 * the following byte make a 15-bit count, so 0xFF 0xFF is the largest hop a
 * node can take. From table 0's base at 0x18E that is 16 hops to leave a
 * 512 KB window.
 *
 * The walk only takes the skip branch when the decision bit is 0, so the
 * region the decoder draws bits from after the table headers is zeroed. That
 * region is the first file's instruction data, which this archive never
 * reaches.
 */
static void make_walk(uint8_t *image)
{
	// Pack the headers tightly so the post-header position is known without
	// having to run a bit reader in here.
	size_t field = FIRST_FIELD;

	for (int i = 0; i < KIPACK_NUM_TABLES; i++)
	{
		put16(image, field, WALK_STRIDE);
		field = field + 2 + WALK_STRIDE;
	}

	// `field` is now the last table's seek target: where the decoder starts
	// drawing decision bits.
	if (field + 64 > ROM_WINDOW_SIZE)
	{
		fprintf(stderr, "mkbadstream: table headers overran the window\n");
		exit(1);
	}
	memset(image + field, 0, 64);

	// The tree itself, at table 0's base.
	size_t node = FIRST_FIELD + 2;

	for (int i = 0; i < 16; i++)
	{
		if (node + 1 >= ROM_WINDOW_SIZE)
		{
			fprintf(stderr, "mkbadstream: walk chain left the window early\n");
			exit(1);
		}
		put16(image, node, 0xFFFF);
		node = node + WALK_HOP;
	}
}

/* Same table-of-modes shape mkromimage uses for its variant layouts. */
static const struct
{
	const char *name;
	void (*build)(uint8_t *image);
} modes[] = {
	{ "magic", make_magic },
	{ "count", make_count },
	{ "size",  make_size  },
	{ "seek",  make_seek  },
	{ "walk",  make_walk  },
};

#define MODE_COUNT (sizeof(modes) / sizeof(modes[0]))

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr,
		        "usage: mkbadstream <in.packed> <magic|count|size|seek|walk> <out.packed>\n");
		return 1;
	}

	void (*build)(uint8_t *) = NULL;
	for (size_t i = 0; i < MODE_COUNT; i++)
	{
		if (strcmp(argv[2], modes[i].name) == 0)
		{
			build = modes[i].build;
			break;
		}
	}
	if (build == NULL)
	{
		fprintf(stderr, "mkbadstream: unknown mode '%s'\n", argv[2]);
		return 1;
	}

	FILE *in = fopen(argv[1], "rb");
	if (in == NULL)
	{
		fprintf(stderr, "mkbadstream: %s: cannot open\n", argv[1]);
		return 1;
	}

	uint8_t *image = calloc(1, ROM_WINDOW_SIZE);
	if (image == NULL)
	{
		fprintf(stderr, "mkbadstream: out of memory\n");
		fclose(in);
		return 1;
	}

	size_t got = fread(image, 1, ROM_WINDOW_SIZE, in);
	int failed = ferror(in);
	fclose(in);

	if (failed || got == 0)
	{
		fprintf(stderr, "mkbadstream: %s: read error\n", argv[1]);
		free(image);
		return 1;
	}

	build(image);

	FILE *out = fopen(argv[3], "wb");
	if (out == NULL)
	{
		fprintf(stderr, "mkbadstream: %s: cannot create\n", argv[3]);
		free(image);
		return 1;
	}
	/* Checked for the same reason mkromimage checks: a short write here makes
	   a differently-broken archive and the failure would land far from here. */
	if (fwrite(image, 1, ROM_WINDOW_SIZE, out) != ROM_WINDOW_SIZE)
	{
		fprintf(stderr, "mkbadstream: %s: short write\n", argv[3]);
		fclose(out);
		free(image);
		return 1;
	}
	if (fclose(out) != 0)
	{
		fprintf(stderr, "mkbadstream: %s: write failed on close\n", argv[3]);
		free(image);
		return 1;
	}

	free(image);
	return 0;
}
