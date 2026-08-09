/**
 * unpack.h - the unpack applet.
 */
#ifndef KIPACK_UNPACK_H
#define KIPACK_UNPACK_H

#include "kipack.h"

/*
 * Decompress the game ROM segments out of a Killer Instinct boot ROM.
 *
 * Writes rom-N.bin and rom-N.addr into out_dir, which must already exist.
 * Pass KIPACK_AUTO to pick the variant from rom_path's file name. A variant
 * outside kipack_variant_t is rejected with -1 rather than indexing out of
 * bounds.
 *
 * Returns 0 on success, -1 on failure with a message already on stderr.
 *
 * Two things worth knowing before embedding this in a larger host:
 *
 *  - It keeps a 512 KB static ROM window (plus 8 bytes of read-ahead padding),
 *    which is a fixed cost of linking the applet in at all. It is .bss, so it
 *    costs address space and touched pages rather than image size, but it
 *    only ever holds the compressed input - each file's decoded output goes
 *    to its own malloc buffer, sized to that file and freed before
 *    kipack_unpack returns.
 *
 *  - The ROM window is global and persists across calls. Loading a ROM clears
 *    it before reading, so a second call with a file shorter than the window
 *    no longer decodes over the previous ROM's tail - but it is still shared
 *    state. Not reentrant, and not safe to call concurrently.
 */
int kipack_unpack(const char *rom_path, const char *out_dir, kipack_variant_t variant);

/* argv shim: [-t variant] <rom> <out_dir>, with argv[0] the applet name. */
int kipack_unpack_main(int argc, char **argv);

#endif /* KIPACK_UNPACK_H */
