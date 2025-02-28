CC = mipsel-linux-gnu-gcc-14
LD = mipsel-linux-gnu-ld
OBJDUMP = mipsel-linux-gnu-objdump
BOARD = 19489
ROM = ki-l15d

KI_BOARD = KI_BOARD_${BOARD}
KI_ROM = $(shell echo $(ROM) | tr '[:lower:]' '[:upper:]' | tr '-' '_')
KI_VARIANT = $(shell echo $(ROM) | cut -d'-' -f1 | tr '[:lower:]' '[:upper:]')

ASM_SOURCES = start.S lzss.S images.S roms.S
ASM_OBJS = $(ASM_SOURCES:%.S=build/${BOARD}-${ROM}-%.o)
C_SOURCES = $(wildcard *.c libs/umm_malloc/*.c)
C_OBJS = $(C_SOURCES:%.c=build/${BOARD}-${ROM}-%.o)
DEPS = $(C_OBJS:.o=.d)

.PHONY: rom
rom: tools images gamerom ${ASM_OBJS} ${C_OBJS}
	mkdir -p output
	$(LD) -Tboot.ld -G 0 -o output/${BOARD}-${ROM}.u98 ${ASM_OBJS} ${C_OBJS}
	$(OBJDUMP) -D output/${BOARD}-${ROM}.u98 -b binary -mmips -M hex > output/${BOARD}-${ROM}.txt

build/${BOARD}-${ROM}-%.o: %.S
	$(CC) -mplt -G 0 -mno-abicalls -mabi=o64 -c -EL -march=r4600 $< -o $@ -I. -D${KI_BOARD} -D${KI_ROM} -D${KI_VARIANT} -DZROM=${ROM}

build/${BOARD}-${ROM}-%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -std=gnu23 -Os -mplt -G 0 -mno-abicalls -mabi=o64 -msym32 -c -EL -march=r4600 $< -o $@ -I. -Ilibs/ -Wall -ffreestanding -D${KI_BOARD} -D${KI_ROM} -D${KI_VARIANT} -DZROM=${ROM} -DHDD_2IN1 -DROM_2IN1
# -mstrict-align

.PHONY: clean
clean:
	-rm -rf build
	-rm -rf tools/**/build
	-rm -f tools/lzss/lzss tools/png2bin/png2bin tools/rom-unpack/rom-unpack tools/fontgen/fontgen tools/disk-checksum/disk-checksum

ROM_BIN = build/roms/${ROM}-0.bin build/roms/${ROM}-1.bin build/roms/${ROM}-2.bin
ROM_ADDR = $(ROM_BIN:.bin=.addr)
ROM_ZBIN = $(ROM_BIN:.bin=.zbin)

.PHONY: gamerom
gamerom: ${ROM_ZBIN}

build/roms/${ROM}-%.bin: assets/roms/${ROM}.u98
	mkdir -p $(@D)
	tools/rom-unpack/rom-unpack ./assets/roms/${ROM}.u98 ./build/roms
	mv build/roms/rom-0.bin build/roms/${ROM}-0.bin
	mv build/roms/rom-0.addr build/roms/${ROM}-0.addr
	mv build/roms/rom-1.bin build/roms/${ROM}-1.bin
	mv build/roms/rom-1.addr build/roms/${ROM}-1.addr
	mv build/roms/rom-2.bin build/roms/${ROM}-2.bin
	mv build/roms/rom-2.addr build/roms/${ROM}-2.addr

build/roms/${ROM}-%.zbin: build/roms/${ROM}-%.bin
	cp $< $@
	tools/lzss/lzss -ewo $@

IMAGES_PNG = $(wildcard assets/images/*.png)
IMAGES_BIN = $(IMAGES_PNG:%.png=build/%.bin)
IMAGES_ZBIN = $(IMAGES_PNG:%.png=build/%.zbin)

.PHONY: images
images: ${IMAGES_ZBIN}

build/assets/images/%.bin: assets/images/%.png
	mkdir -p $(@D)
	tools/png2bin/png2bin $< build/

build/assets/images/%.zbin: build/assets/images/%.bin
	cp $< $@
	tools/lzss/lzss -ewo $@

.PHONY: tools
tools: tools/lzss/lzss tools/png2bin/png2bin tools/rom-unpack/rom-unpack tools/fontgen/fontgen tools/disk-checksum/disk-checksum

tools/lzss/lzss:
	cd tools/lzss && make

tools/png2bin/png2bin:
	cd tools/png2bin && make

tools/rom-unpack/rom-unpack:
	cd tools/rom-unpack && make

tools/fontgen/fontgen:
	cd tools/fontgen && make

tools/disk-checksum/disk-checksum:
	cd tools/disk-checksum && make

build/${BOARD}-${ROM}-print.o: font.h
font.h:
	tools/fontgen/fontgen > $@

-include $(DEPS)

.PHONY: roms
roms:
# 19489 KI1
	make rom BOARD=19489 ROM=ki-l13
	make rom BOARD=19489 ROM=ki-l14
	make rom BOARD=19489 ROM=ki-l15d
	make rom BOARD=19489 ROM=ki-l15di
# 19489 KI2
	make rom BOARD=19489 ROM=ki2-l10
	make rom BOARD=19489 ROM=ki2-l11
	make rom BOARD=19489 ROM=ki2-l13k
	make rom BOARD=19489 ROM=ki2-l14k
# 20351 KI1
	make rom BOARD=20351 ROM=ki-l13
	make rom BOARD=20351 ROM=ki-l14
	make rom BOARD=20351 ROM=ki-l15d
	make rom BOARD=20351 ROM=ki-l15di
# 20351 KI2
	make rom BOARD=20351 ROM=ki2-l10
	make rom BOARD=20351 ROM=ki2-l11
	make rom BOARD=20351 ROM=ki2-l13
	make rom BOARD=20351 ROM=ki2-l14

.PHONY: hdd
hdd:
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
