/**
 * kipack.h - format constants and ROM-variant helpers shared by the
 * pack and unpack applets.
 */
#ifndef KIPACK_H
#define KIPACK_H

#include <string.h>

/* Bitstream header magic: "br", for binary ROM. */
#define KIPACK_MAGIC 0x7262

/* Number of Huffman tables written per compressed file. */
#define KIPACK_NUM_TABLES 24

/*
 * Layout of a .packed file produced by the pack applet. unpack reads the same
 * offsets back, so keeping both applets on these constants is what stops the
 * two sides drifting apart.
 */
#define KIPACK_PACKED_FUNCT    0x0000
#define KIPACK_PACKED_REGIMM   0x0080
#define KIPACK_PACKED_EXTENDED 0x00C0
#define KIPACK_PACKED_OPCODE   0x0100
#define KIPACK_PACKED_DATA     0x0180

/* Index order matches rom_offsets[] in unpack.c - do not reorder. */
typedef enum
{
	KIPACK_KI1     = 0,
	KIPACK_KI1_P47 = 1,
	KIPACK_KI2     = 2,
	KIPACK_PACKED  = 3,
	KIPACK_AUTO    = 4,
} kipack_variant_t;

/* "ki2" -> KIPACK_KI2. Returns KIPACK_AUTO when the name is not recognised. */
static inline kipack_variant_t kipack_variant_parse(const char *name)
{
	if (strcmp(name, "ki1") == 0)     return KIPACK_KI1;
	if (strcmp(name, "ki1-p47") == 0) return KIPACK_KI1_P47;
	if (strcmp(name, "ki2") == 0)     return KIPACK_KI2;
	if (strcmp(name, "packed") == 0)  return KIPACK_PACKED;
	return KIPACK_AUTO;
}

/*
 * Guess the variant from the file name. This is the heuristic the original
 * unpack tool used, preserved verbatim - including the order of the tests -
 * so existing command lines keep selecting the same tables.
 */
static inline kipack_variant_t kipack_variant_sniff(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *base = slash ? slash + 1 : path;

	if (strncmp(base, "packed", 6) == 0 || strstr(base, ".packed") != NULL)
		return KIPACK_PACKED;
	if (strncmp(base, "ki2-", 4) == 0 || strncmp(base, "ki2_", 4) == 0)
		return KIPACK_KI2;
	if (strncmp(base, "ki-p47", 6) == 0)
		return KIPACK_KI1_P47;
	return KIPACK_KI1;
}

#endif /* KIPACK_H */
