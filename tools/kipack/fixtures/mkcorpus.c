/**
 * mkcorpus.c - emit a synthetic MIPS corpus for the kipack round-trip tests.
 *
 * These instructions are ours, not Rare's, so the fixture is committable. Each
 * word drives a specific branch of the format-flag dispatch in pack/unpack; the
 * comment gives the flags it reaches.
 *
 * Two branches are deliberately absent because the shipped tables cannot
 * produce them: 0x4000 (the JR $ra special case) and 0x2000 (the NOP special
 * case). No table entry holds those values.
 *
 * 0x200 (MTF table 2, TABLE_RS_MTF2) is NOT in that list - it IS reachable: it
 * is a sub-flag of 0x0605, the format flags shared by every load/store opcode
 * (26, 27, 32-50, 52-58, 60-63 - 51 is PREF and 59 is reserved, both 0x0000),
 * so rs on those instructions already routes through MTF table 2. The lw/sw
 * entries below exercise it; see their comments.
 *
 * Usage: mkcorpus <out_dir>
 */
#include <stdio.h>
#include <stdint.h>

/* Segment 0: one instruction per encoder branch. */
static const uint32_t segment0[] = {
	0x00000000,  /* nop          funct 0x00 -> 0x0056, rt/rd MTF1 + shamt      */
	0x00094100,  /* sll   t0,t1,4    funct 0x00 -> 0x0056                      */
	0x00094102,  /* srl   t0,t1,4    funct 0x02 -> 0x0056                      */
	0x00094103,  /* sra   t0,t1,4    funct 0x03 -> 0x0056                      */
	0x0009413c,  /* dsll32 t0,t1,4   funct 0x3c -> 0x0056                      */
	0x01494004,  /* sllv  t0,t1,t2   funct 0x04 -> 0x0095, rs/rt/rd MTF1       */
	0x012a4021,  /* addu  t0,t1,t2   funct 0x21 -> 0x0095                      */
	0x012a4024,  /* and   t0,t1,t2   funct 0x24 -> 0x0095                      */
	0x012a402b,  /* sltu  t0,t1,t2   funct 0x2b -> 0x0095                      */
	0x0320f809,  /* jalr  ra,t9      funct 0x09 -> 0x0099, rs/rd MTF1          */
	0x00004010,  /* mfhi  t0         funct 0x10 -> 0x009a, rd MTF1             */
	0x00004812,  /* mflo  t1         funct 0x12 -> 0x009a                      */
	0x01090018,  /* mult  t0,t1      funct 0x18 -> 0x00a5, rs/rt MTF1          */
	0x0109001b,  /* divu  t0,t1      funct 0x1b -> 0x00a5                      */
	0x03e00008,  /* jr    ra         funct 0x08 -> 0x00a9; the pass-desync case*/
	0x03200008,  /* jr    t9         funct 0x08 -> 0x00a9                      */
	0x01000011,  /* mthi  t0         funct 0x11 -> 0x00a9                      */
	0x0000000c,  /* syscall          funct 0x0c -> 0, raw 20-bit               */
	0x0000000d,  /* break            funct 0x0d -> 0, raw 20-bit               */
	0x05000001,  /* bltz  t0,+4      regimm 0  -> 0x0c01, rs MTF1 + imm t3     */
	0x05010001,  /* bgez  t0,+4      regimm 1  -> 0x0c01                       */
	0x05080005,  /* tgei  t0,5       regimm 8  -> 0x0801, imm t2               */
	0x05051234,  /* regimm rt=5   -> 0, reserved: rs direct + imm t1           */
	/*
	 * These two share a target - 0x09234567 & 0x03ffffff == 0x0d234567 &
	 * 0x03ffffff == 0x1234567 - so each of TABLE_JUMP_0..3 sees exactly one
	 * distinct symbol across the whole corpus. That is what covers the
	 * single-symbol Huffman table, whose code is 0 bits wide rather than 1,
	 * and it is the only thing that covers it on a clone with no ROMs.
	 * Changing either target to differ from the other silently removes that
	 * coverage.
	 */
	0x09234567,  /* j     ...        opcode 2  -> 0x0100, 4-part target        */
	0x0d234567,  /* jal   ...        opcode 3  -> 0x0100                       */
	0x11090001,  /* beq   t0,t1,+4   opcode 4  -> 0x0c05, imm t3               */
	0x15090001,  /* bne   t0,t1,+4   opcode 5  -> 0x0c05                       */
	0x19000001,  /* blez  t0,+4      opcode 6  -> 0x0c09, rt direct            */
	0x1d000001,  /* bgtz  t0,+4      opcode 7  -> 0x0c09                       */
	0x25281234,  /* addiu t0,t1,..   opcode 9  -> 0x0805, imm t2               */
	0x35285678,  /* ori   t0,t1,..   opcode 13 -> 0x0805                       */
	0x3c088001,  /* lui   t0,0x8001  opcode 15 -> 0x0806, rs direct            */
	0x8d280010,  /* lw    t0,16(t1)  opcode 35 -> 0x0605, imm t1, rs MTF2      */
	0xad280014,  /* sw    t0,20(t1)  opcode 43 -> 0x0605, rs MTF2              */
	0x40086000,  /* mfc0  t0,$12     opcode 16, ext 0  -> 0x1094, imm direct   */
	0x40886000,  /* mtc0  t0,$12     opcode 16, ext 4  -> 0x1094               */
	0x41000001,  /* bc0f  +4         opcode 16, ext 8  -> 0x0c00, rt<4         */
	0x41040002,  /* bc0   rt=4       opcode 16, ext 8  -> 0x0c00, rt>=4        */
	0x42000018,  /* eret             opcode 16, ext 16 -> 0xffff, raw 21-bit   */
	0x40681234,  /* cop0  ext 3   -> 0, reserved: rs direct + imm t1           */
	0x70123456,  /* opcode 28     -> 0, raw 26-bit                             */
	0x4c123456,  /* opcode 19     -> 0, raw 26-bit                             */
};

/* Segment 1: a second file, to exercise the multi-file loop and per-file tables. */
static const uint32_t segment1[] = {
	0x3c088001,  /* lui   t0,0x8001                                            */
	0x25281234,  /* addiu t0,t1,0x1234                                         */
	0x8d280010,  /* lw    t0,16(t1)                                            */
	0x03e00008,  /* jr    ra                                                   */
	0x00000000,  /* nop                                                        */
};

/*
 * Segment 2: 256 addiu words whose immediates run 0x0000..0x00ff.
 *
 * Segments 0 and 1 are 47 words between them, which is far too few leaves to
 * make any Huffman subtree reach 128 serialized bytes - so nothing above ever
 * reaches the 2-byte skip count that pack writes (and unpack reads back) for a
 * right subtree that large. Every real ROM uses that path, so without this
 * segment it would go entirely untested on a fresh clone, where the .u98 files
 * under assets/roms are never present.
 *
 * addiu is opcode 9 -> 0x0805, whose 16-bit immediate is split across plain
 * Huffman tables IMM_HI_3 and IMM_LO_3 (0x0805 & 0xC00 == 0x800 selects that
 * pair; the immediate bytes are counted raw, with no MTF transform). Holding
 * the high byte at 0x00 and sweeping the low byte gives IMM_LO_3 all 256
 * symbols, hence 256 two-byte leaves, which puts the root's right subtree
 * well past the threshold.
 *
 * addiu t0,t1,imm = opcode 9, rs=t1(9), rt=t0(8), i.e. 0x25280000 | imm - the
 * same encoding as the 0x25281234 entry in segment 0.
 */
#define SEGMENT2_COUNT 256

static void build_segment2(uint32_t *words)
{
	for (uint32_t i = 0; i < SEGMENT2_COUNT; i++)
	{
		words[i] = 0x25280000u | i;
	}
}

static int write_segment(const char *dir, int id, const uint32_t *words,
                         size_t count, uint32_t load_addr)
{
	char path[512];
	FILE *fd;

	snprintf(path, sizeof(path), "%s/rom-%d.bin", dir, id);
	fd = fopen(path, "wb");
	if (fd == NULL)
	{
		fprintf(stderr, "mkcorpus: %s: cannot create\n", path);
		return -1;
	}
	for (size_t i = 0; i < count; i++)
	{
		uint8_t le[4] = {
			(uint8_t)(words[i] & 0xff),
			(uint8_t)((words[i] >> 8) & 0xff),
			(uint8_t)((words[i] >> 16) & 0xff),
			(uint8_t)((words[i] >> 24) & 0xff),
		};
		/* Unchecked, a short write here yields a truncated fixture that the
		   round-trip tests would then compare against - and pass. */
		if (fwrite(le, 1, 4, fd) != 4)
		{
			fprintf(stderr, "mkcorpus: %s: short write\n", path);
			fclose(fd);
			return -1;
		}
	}
	/* Buffered data is flushed here, so a full disk can surface at close. */
	if (fclose(fd) != 0)
	{
		fprintf(stderr, "mkcorpus: %s: write failed on close\n", path);
		return -1;
	}

	snprintf(path, sizeof(path), "%s/rom-%d.addr", dir, id);
	fd = fopen(path, "wb");
	if (fd == NULL)
	{
		fprintf(stderr, "mkcorpus: %s: cannot create\n", path);
		return -1;
	}
	uint8_t le[4] = {
		(uint8_t)(load_addr & 0xff),
		(uint8_t)((load_addr >> 8) & 0xff),
		(uint8_t)((load_addr >> 16) & 0xff),
		(uint8_t)((load_addr >> 24) & 0xff),
	};
	if (fwrite(le, 1, 4, fd) != 4)
	{
		fprintf(stderr, "mkcorpus: %s: short write\n", path);
		fclose(fd);
		return -1;
	}
	if (fclose(fd) != 0)
	{
		fprintf(stderr, "mkcorpus: %s: write failed on close\n", path);
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: mkcorpus <out_dir>\n");
		return 1;
	}

	/* Physical DRAM addresses; pack turns these into KSEG0 by setting bit 31. */
	if (write_segment(argv[1], 0, segment0,
	                  sizeof(segment0) / sizeof(segment0[0]), 0x08010000) != 0)
		return 1;
	if (write_segment(argv[1], 1, segment1,
	                  sizeof(segment1) / sizeof(segment1[0]), 0x08020000) != 0)
		return 1;

	uint32_t segment2[SEGMENT2_COUNT];
	build_segment2(segment2);
	if (write_segment(argv[1], 2, segment2, SEGMENT2_COUNT, 0x08030000) != 0)
		return 1;

	return 0;
}
