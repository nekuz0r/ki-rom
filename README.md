# Killer instinct Boot ROM

This project is an open source Boot ROM for Killer instinct 1 & 2 arcade game developed by Rare and published by Midway.

## Compatibility

| ROM version | Board version   | Status | Note                  |
| ----------- | --------------- | ------ | --------------------- |
| KI l1.5di   | KI-U96 A-19489  | OK     |                       |
| KI l1.5d    | KI-U96 A-19489  | P      | need ide patch        |
| KI l1.4     | KI-U96 A-19489  | P      | need ide patch        |
| KI l1.3     | KI-U96 A-19489  | P      | need ide patch        |
| KI p47      | KI-U96 A-19489  | NOK    | unpack not working    |
| KI2 l1.4    | KI-U96 A-19489  | NOK    | use KI2 l1.4k         |
| KI2 l1.4k   | KI-U96 A-19489  | OK     |                       |
| KI l1.5di   | KI2-U96 A-20351 | OK     |                       |
| KI l1.5d    | KI2-U96 A-20351 | NOK    | use KI l1.5di instead |
| KI l1.4     | KI2-U96 A-20351 | NOK    | need remap+ide patch  |
| KI l1.3     | KI2-U96 A-20351 | NOK    | need remap+ide patch  |
| KI p47      | KI2-U96 A-20351 | NOK    | unpack not working    |
| KI2 l1.4    | KI2-U96 A-20351 | OK     |                       |
| KI2 l1.4k   | KI2-U96 A-20351 | NOK    | use KI2 l1.4 instead  |

- OK: Supported
- NOK: Not Supported
- P: Partially Supported (requires full original hardware)

## Features

This boot rom introduces new features compared to the stock boot rom.

- LZSS compression/decompression of game ROM (faster boot time)
- Self test (SRAM, VRAM, DRAM, IDE) (S1:8 to pause boot)
- In-memory patching of game ROM
- Fixes no sound at boot on MAME (if S1:7 is enabled)
- Soft Multiboot K1 & K2 (Requires additional hardware & compilation flag)
- Additional dipswitch configuration bits

## Soft Multiboot

This ROM version adds the ability to boot multiple game ROMs (KI1 + KI2) from the same board and to switch from one to the other without requiring to toggle a physical switch and power cycling.

The default game rom is set with the S1:6 dipswitch bit (Off = K1, On = KI2).

When performing a soft reset a selection screen is displayed, pressing P1 START will start KI1
and pressing P2 START will start KI2.

## Patches

### KI2-U96 A-20351 remap

This patch remaps I/O memory addresses to allow KI1 ROM to run on KI2 dedicated hardware.

### K12-U1 A-20383 protection bypass

This patch disable the copy protection of the KI1 to KI2 upgrade kit.

### Any IDE

This patch disable the IDE drive model check, allowing to run the game with any IDE drive.

### Soft reset

This patch adds an input combination allowing to soft reset the game by pressing
P1 LEFT + P1 START + P2 RIGHT + P2 Start.

## DipSwitch S1

| bit | description               | Off | On  |
| --- | ------------------------- | --- | --  |
| 6   | Select default game ROM   | KI1 | KI2 |
| 7   | Disable sound test (Bong) | Off | On  |
| 8   | Wait for input on POST    | Off | On  |

## To do

- Enter infinite loop if a self test failed
- Write remap & ide patch for KI l1.5d
- Write remap & ide patch for KI l1.4
- Write remap & ide patch for KI l1.3
- Include KI2 1.0, 1.1, 1.3, 1.3k
- Add support for KI1 p47 unpack

## Acknowledgments

- [github.com/DS-Homebrew](https://github.com/DS-Homebrew/nds-bootstrap/blob/master/lzss.c) for the LZSS ROM compression tool
- [github.com/PeterLemon](https://github.com/PeterLemon/N64/blob/master/Compress/LZ77/LZ77Decode/LZ77Decode.asm) for the LZSS mips assembly decompression
- [github.com/azmr](https://github.com/azmr/blit-fonts/blob/master/src/blit32_glyphs.h) for the 5x6 bitmap font
- [arcade-projects.com](https://www.arcade-projects.com/) members for their support and great ideas
- My wife for letting me spending evenings and nights on this projet
