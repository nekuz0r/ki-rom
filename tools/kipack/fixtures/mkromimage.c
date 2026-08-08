/**
 * mkromimage.c - wrap a .packed file into a fake boot ROM image.
 *
 * Reads the four lookup tables and the bitstream out of a .packed file and
 * rewrites them at the offsets a real KI boot ROM uses, so that `kipack unpack`
 * can be tested against every ROM variant without any ROM data. Every byte in
 * the output came from the pack applet.
 *
 * Usage: mkromimage <in.packed> <ki1|ki1-p47|ki2> <out.u98>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../kipack.h"

#define ROM_SIZE 0x80000

typedef struct
{
	const char *name;
	uint16_t compressed_data;
	uint16_t opcode_table;
	uint16_t funct_table;
	uint16_t regimm_table;
	uint16_t extended_table;
} variant_layout_t;

/* Mirrors rom_offsets[] in unpack.c for the three real ROM variants. */
static const variant_layout_t layouts[] = {
	{ "ki1",     0x0fd0, 0x0f48, 0x0e48, 0x0ec8, 0x0f08 },
	{ "ki1-p47", 0x1010, 0x0f80, 0x0e80, 0x0f00, 0x0f40 },
	{ "ki2",     0x1040, 0x0fb0, 0x0eb0, 0x0f30, 0x0f70 },
};

#define LAYOUT_COUNT (sizeof(layouts) / sizeof(layouts[0]))

#define FUNCT_SIZE    128
#define REGIMM_SIZE    64
#define EXTENDED_SIZE  64
#define OPCODE_SIZE   128

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "usage: mkromimage <in.packed> <ki1|ki1-p47|ki2> <out.u98>\n");
		return 1;
	}

	const variant_layout_t *layout = NULL;
	for (size_t i = 0; i < LAYOUT_COUNT; i++)
	{
		if (strcmp(argv[2], layouts[i].name) == 0)
		{
			layout = &layouts[i];
			break;
		}
	}
	if (layout == NULL)
	{
		fprintf(stderr, "mkromimage: unknown variant '%s'\n", argv[2]);
		return 1;
	}

	FILE *in = fopen(argv[1], "rb");
	if (in == NULL)
	{
		fprintf(stderr, "mkromimage: %s: cannot open\n", argv[1]);
		return 1;
	}
	fseek(in, 0, SEEK_END);
	long in_size = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (in_size < 0)
	{
		fprintf(stderr, "mkromimage: %s: cannot determine size\n", argv[1]);
		fclose(in);
		return 1;
	}

	uint8_t *packed = malloc(in_size);
	if (packed == NULL || fread(packed, 1, in_size, in) != (size_t)in_size)
	{
		fprintf(stderr, "mkromimage: %s: short read\n", argv[1]);
		fclose(in);
		free(packed);
		return 1;
	}
	fclose(in);

	long stream_size = in_size - KIPACK_PACKED_DATA;
	if (stream_size < 0 || layout->compressed_data + stream_size > ROM_SIZE)
	{
		fprintf(stderr, "mkromimage: %s does not fit a %d byte image\n",
		        argv[1], ROM_SIZE);
		free(packed);
		return 1;
	}

	uint8_t *image = calloc(1, ROM_SIZE);
	if (image == NULL)
	{
		fprintf(stderr, "mkromimage: out of memory\n");
		free(packed);
		return 1;
	}

	memcpy(image + layout->funct_table,    packed + KIPACK_PACKED_FUNCT,    FUNCT_SIZE);
	memcpy(image + layout->regimm_table,   packed + KIPACK_PACKED_REGIMM,   REGIMM_SIZE);
	memcpy(image + layout->extended_table, packed + KIPACK_PACKED_EXTENDED, EXTENDED_SIZE);
	memcpy(image + layout->opcode_table,   packed + KIPACK_PACKED_OPCODE,   OPCODE_SIZE);
	memcpy(image + layout->compressed_data, packed + KIPACK_PACKED_DATA, stream_size);

	FILE *out = fopen(argv[3], "wb");
	if (out == NULL)
	{
		fprintf(stderr, "mkromimage: %s: cannot create\n", argv[3]);
		free(image);
		free(packed);
		return 1;
	}
	/* A short write here would produce a truncated .u98 that unpack reads as
	   a valid but wrong ROM, so the check would fail somewhere far from the
	   real cause. fclose is checked too: this is where the flush happens. */
	if (fwrite(image, 1, ROM_SIZE, out) != ROM_SIZE)
	{
		fprintf(stderr, "mkromimage: %s: short write\n", argv[3]);
		fclose(out);
		free(image);
		free(packed);
		return 1;
	}
	if (fclose(out) != 0)
	{
		fprintf(stderr, "mkromimage: %s: write failed on close\n", argv[3]);
		free(image);
		free(packed);
		return 1;
	}

	free(image);
	free(packed);
	return 0;
}
