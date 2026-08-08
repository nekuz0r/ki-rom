/**
 * pack.h - the pack applet.
 */
#ifndef KIPACK_PACK_H
#define KIPACK_PACK_H

#include "kipack.h"

/*
 * Compress rom-N.bin / rom-N.addr pairs from in_dir into out_path.
 *
 * Reads rom-0 upwards and stops at the first missing pair. max_files caps how
 * many are considered and is clamped to the internal maximum of 16.
 *
 * Returns 0 on success, -1 on failure with a message already on stderr.
 */
int kipack_pack(const char *in_dir, const char *out_path, int max_files);

/* argv shim: <in_dir> <out.packed> [count], with argv[0] the applet name. */
int kipack_pack_main(int argc, char **argv);

#endif /* KIPACK_PACK_H */
