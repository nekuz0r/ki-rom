/**
 * main.c - multi-call dispatcher for the kipack applets.
 *
 * Each applet lives in its own translation unit and exports an argv entry
 * point. Adding an applet to another tool means compiling that one .c file
 * and adding a row to that tool's own table.
 */
#include <stdio.h>
#include <string.h>

#include "pack.h"
#include "unpack.h"

typedef struct
{
	const char *name;
	int (*fn)(int argc, char **argv);
	const char *usage;
} kipack_applet_t;

static const kipack_applet_t applets[] = {
	{ "pack",   kipack_pack_main,   "<in_dir> <out.packed> [count]" },
	{ "unpack", kipack_unpack_main, "[-t variant] <rom> <out_dir>"  },
};

#define APPLET_COUNT (sizeof(applets) / sizeof(applets[0]))

static int usage(const char *argv0)
{
	fprintf(stderr, "usage: %s <command> [args]\n\n", argv0);
	fprintf(stderr, "commands:\n");
	for (size_t i = 0; i < APPLET_COUNT; i++)
		fprintf(stderr, "  %-8s %s\n", applets[i].name, applets[i].usage);
	fprintf(stderr, "\nvariants: ki1, ki1-p47, ki2, packed\n");
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return usage(argv[0]);

	for (size_t i = 0; i < APPLET_COUNT; i++)
	{
		if (strcmp(argv[1], applets[i].name) == 0)
			return applets[i].fn(argc - 1, argv + 1);
	}

	fprintf(stderr, "%s: unknown command '%s'\n", argv[0], argv[1]);
	return usage(argv[0]);
}
