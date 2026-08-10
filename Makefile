CC = mipsel-linux-gnu-gcc-14
LD = mipsel-linux-gnu-ld
OBJDUMP = mipsel-linux-gnu-objdump
OBJCOPY = mipsel-linux-gnu-objcopy
BOARD = 19489
ROM = ki-l15d
BOOT_VIEW = view_main

# Without this, an interrupted or failing recipe leaves its half-written target
# on disk. That is actively dangerous for the .zbin rules, which "cp" the raw
# file into place before compressing it: a failed lzss leaves an uncompressed
# file named .zbin, newer than its prerequisite, so the next make considers it
# up to date and links raw pixel data into the ROM as if it were compressed.
.DELETE_ON_ERROR:

KI_BOARD = KI_BOARD_${BOARD}
KI_ROM = $(shell echo $(ROM) | tr '[:lower:]' '[:upper:]' | tr '-' '_')
KI_VARIANT = $(shell echo $(ROM) | cut -d'-' -f1 | tr '[:lower:]' '[:upper:]')

ASM_SOURCES = start.S lzss.S images.S roms.S
ASM_OBJS = $(ASM_SOURCES:%.S=build/${BOARD}-${ROM}-%.o)
C_SOURCES = $(wildcard *.c libs/umm_malloc/*.c)
C_OBJS = $(C_SOURCES:%.c=build/${BOARD}-${ROM}-%.o)
DEPS = $(C_OBJS:.o=.d)

# rom depends on the .elf as a real file, not on check-ramcode as a phony
# target: check-ramcode itself needs to build that same .elf, and a phony-to-
# phony dependency in the other direction would make the two circular. Routing
# both through the file lets each run the check exactly once from its own
# recipe below, whether reached via `rom` or invoked standalone.
.PHONY: rom
rom: build/${BOARD}-${ROM}.elf
	OBJDUMP=$(OBJDUMP) tools/check-ramcode.sh build/${BOARD}-${ROM}.elf

# Grouped target (&:, GNU make >= 4.3): ld, objcopy and objdump run once and
# produce all three files together. A plain multi-target rule leaves make free
# to treat them as independent under `make -j`, which can run this recipe more
# than once for the same build and race the two writes to the .elf.
build/${BOARD}-${ROM}.elf output/${BOARD}-${ROM}.u98 output/${BOARD}-${ROM}.txt &: tools images gamerom ${ASM_OBJS} ${C_OBJS}
	mkdir -p output
	$(LD) -Tboot.ld -G 0 --no-undefined -o build/${BOARD}-${ROM}.elf ${ASM_OBJS} ${C_OBJS}
	$(OBJCOPY) -O binary build/${BOARD}-${ROM}.elf output/${BOARD}-${ROM}.u98
	$(OBJDUMP) -D output/${BOARD}-${ROM}.u98 -b binary -mmips -M hex > output/${BOARD}-${ROM}.txt

build/${BOARD}-${ROM}-%.o: %.S
	mkdir -p $(@D)
	$(CC) -mplt -G 0 -mno-abicalls -mabi=o64 -c -EL -march=r4600 $< -o $@ -I. -D${KI_BOARD} -D${KI_ROM} -D${KI_VARIANT} -DZROM=${ROM} -DKI_BOOT_VIEW=${BOOT_VIEW}

# BOOT_VIEW only ever arrives on the command line, so nothing on disk changes
# when it does and start.o would keep whatever view it was first built with.
# This stamp holds the current value and is rewritten only when it differs, so
# it is a prerequisite that goes stale exactly when the setting does.
BOOT_VIEW_STAMP = build/${BOARD}-${ROM}-bootview.stamp

.PHONY: force
force:

${BOOT_VIEW_STAMP}: force
	@mkdir -p $(@D)
	@echo '${BOOT_VIEW}' | cmp -s - $@ || echo '${BOOT_VIEW}' > $@

build/${BOARD}-${ROM}-start.o: ${BOOT_VIEW_STAMP}

build/${BOARD}-${ROM}-%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -MP -std=gnu23 -Os -mplt -G 0 -mno-abicalls -mabi=o64 -msym32 -c -EL -march=r4600 $< -o $@ -I. -Ilibs/ -Wall -ffreestanding -D${KI_BOARD} -D${KI_ROM} -D${KI_VARIANT} -DZROM=${ROM} 
#-DHDD_2IN1 -DROM_2IN1
# -mstrict-align

# A .ramcode function keeps running while the ROM window holds a different
# image, so a single reach back into ROM is fatal and nothing in the toolchain
# catches it. This is that check -- `rom` above already runs it against every
# build; this target exists so it can also be run standalone against an .elf
# that is already built (or built here, if it is not).
.PHONY: check-ramcode
check-ramcode: build/${BOARD}-${ROM}.elf
	OBJDUMP=$(OBJDUMP) tools/check-ramcode.sh build/${BOARD}-${ROM}.elf

.PHONY: check-ramcode-all
check-ramcode-all:
	@for f in build/*.elf; do OBJDUMP=$(OBJDUMP) tools/check-ramcode.sh $$f || exit 1; done

.PHONY: clean
clean:
	-rm -rf build
	-rm -rf tools/**/build
	-rm -f font.h
	-rm -f tools/lzss/lzss tools/png2bin/png2bin tools/gif2bin/gif2bin tools/fontgen/fontgen tools/disk-checksum/disk-checksum tools/kipack/kipack

ROM_BIN = build/roms/${ROM}-0.bin build/roms/${ROM}-1.bin build/roms/${ROM}-2.bin
ROM_ADDR = $(ROM_BIN:.bin=.addr)
ROM_ZBIN = $(ROM_BIN:.bin=.zbin)

.PHONY: gamerom
gamerom: ${ROM_ZBIN}

# The order-only prerequisites on the asset and segment rules below exist for
# the same reason as this one: `rom:` lists `tools` before the targets that
# invoke them, and under `make -j` that ordering means nothing. Without them a
# recipe can run a tool binary while the `tools` target is still linking it.
#
# Grouped target (&:, GNU make >= 4.3): one invocation of kipack unpack
# produces all six files. With a normal pattern rule make treats each output
# as its own target, so `make -j` runs kipack unpack concurrently three times
# into the same build/roms/rom-{0,1,2}.bin and the mv commands race --
# silently producing corrupt segments that are then compressed and linked
# into the ROM.
${ROM_BIN} ${ROM_ADDR} &: assets/roms/${ROM}.u98 | tools/kipack/kipack
	mkdir -p $(@D)
	tools/kipack/kipack unpack ./assets/roms/${ROM}.u98 ./build/roms
	mv build/roms/rom-0.bin build/roms/${ROM}-0.bin
	mv build/roms/rom-0.addr build/roms/${ROM}-0.addr
	mv build/roms/rom-1.bin build/roms/${ROM}-1.bin
	mv build/roms/rom-1.addr build/roms/${ROM}-1.addr
	mv build/roms/rom-2.bin build/roms/${ROM}-2.bin
	mv build/roms/rom-2.addr build/roms/${ROM}-2.addr

build/roms/${ROM}-%.zbin: build/roms/${ROM}-%.bin | tools/lzss/lzss
	cp $< $@
	tools/lzss/lzss -ewo $@

IMAGES_PNG = $(wildcard assets/images/*.png)
IMAGES_GIF = $(wildcard assets/images/*.gif)
IMAGES_BIN = $(IMAGES_PNG:%.png=build/%.bin) $(IMAGES_GIF:%.gif=build/%.bin)
IMAGES_ZBIN = $(IMAGES_PNG:%.png=build/%.zbin) $(IMAGES_GIF:%.gif=build/%.zbin)

.PHONY: images
images: ${IMAGES_ZBIN}

build/assets/images/%.bin: assets/images/%.png | tools/png2bin/png2bin
	mkdir -p $(@D)
	tools/png2bin/png2bin $< build/

build/assets/images/%.bin: assets/images/%.gif | tools/gif2bin/gif2bin
	mkdir -p $(@D)
	tools/gif2bin/gif2bin $< build/

build/assets/images/%.zbin: build/assets/images/%.bin | tools/lzss/lzss
	cp $< $@
	tools/lzss/lzss -ewo $@

.PHONY: tools
tools: tools/lzss/lzss tools/png2bin/png2bin tools/gif2bin/gif2bin tools/fontgen/fontgen tools/disk-checksum/disk-checksum tools/kipack/kipack

tools/lzss/lzss: $(wildcard tools/lzss/*.c tools/lzss/*.h) tools/lzss/Makefile
	$(MAKE) -C tools/lzss

tools/png2bin/png2bin: $(wildcard tools/png2bin/*.c tools/png2bin/*.h) tools/png2bin/Makefile
	$(MAKE) -C tools/png2bin

tools/gif2bin/gif2bin: $(wildcard tools/gif2bin/*.c tools/gif2bin/*.h) tools/gif2bin/Makefile
	$(MAKE) -C tools/gif2bin

tools/fontgen/fontgen: $(wildcard tools/fontgen/*.c tools/fontgen/*.h) tools/fontgen/Makefile
	$(MAKE) -C tools/fontgen

tools/disk-checksum/disk-checksum: $(wildcard tools/disk-checksum/*.c tools/disk-checksum/*.h) tools/disk-checksum/Makefile
	$(MAKE) -C tools/disk-checksum

tools/kipack/kipack: $(wildcard tools/kipack/*.c tools/kipack/*.h) tools/kipack/Makefile
	$(MAKE) -C tools/kipack

build/${BOARD}-${ROM}-print.o: font.h
font.h: tools/fontgen/fontgen
	tools/fontgen/fontgen > $@

# cpp cannot see through .incbin, so -MMD does not cover the assets that
# images.S and roms.S embed. Without these, editing a PNG regenerates its
# .zbin but does not reassemble the object, and the ROM silently keeps the
# previous artwork until `make clean`.
build/${BOARD}-${ROM}-images.o: ${IMAGES_ZBIN}
build/${BOARD}-${ROM}-roms.o: ${ROM_ZBIN} ${ROM_ADDR}

-include $(DEPS)

.PHONY: roms
roms:
# 19489 KI1
	$(MAKE) rom BOARD=19489 ROM=ki-l13
	$(MAKE) rom BOARD=19489 ROM=ki-l14
	$(MAKE) rom BOARD=19489 ROM=ki-l15d
	$(MAKE) rom BOARD=19489 ROM=ki-l15di
# 19489 KI2
	$(MAKE) rom BOARD=19489 ROM=ki2-l10
	$(MAKE) rom BOARD=19489 ROM=ki2-l11
	$(MAKE) rom BOARD=19489 ROM=ki2-l13k
	$(MAKE) rom BOARD=19489 ROM=ki2-l14k
# 20351 KI1
	$(MAKE) rom BOARD=20351 ROM=ki-l13
	$(MAKE) rom BOARD=20351 ROM=ki-l14
	$(MAKE) rom BOARD=20351 ROM=ki-l15d
	$(MAKE) rom BOARD=20351 ROM=ki-l15di
# 20351 KI2
	$(MAKE) rom BOARD=20351 ROM=ki2-l10
	$(MAKE) rom BOARD=20351 ROM=ki2-l11
	$(MAKE) rom BOARD=20351 ROM=ki2-l13
	$(MAKE) rom BOARD=20351 ROM=ki2-l14

.PHONY: hdd
hdd:
	mkdir -p output
	cat assets/disks/ki1.hd.bin assets/disks/ki2.hd.bin > output/hdd.bin
	truncate --size=340802560 output/hdd.bin

.PHONY: mame
mame: rom
ifeq ($(BOARD), 19489)
	cp output/${BOARD}-${ROM}.u98 /mnt/kinst/ki-l15d.u98
endif
ifeq ($(BOARD), 20351)
	cp output/${BOARD}-${ROM}.u98 /mnt/kinst2/ki2-l14.u98
endif
