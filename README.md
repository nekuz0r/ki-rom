# Killer instinct Boot ROM

This project is an open source Boot ROM for Killer instinct 1 & 2 arcade game developed by Rare and published by Midway.

## Compatibility

### KI-U96 A-19489

| ROM version | Status             | Note                     |
| ----------- | ------------------ | ------------------------ |
| KI l1.5di   | :white_check_mark: |                          |
| KI l1.5d    | :warning:          | need any-ide patch       |
| KI l1.4     | :warning:          | need any-ide patch       |
| KI l1.3     | :warning:          | need any-ide patch       |
| KI p47      | :warning:          | need any-ide patch       |
| KI2 l1.0    | :no_entry_sign:    | need remap patch         |
| KI2 l1.1    | :no_entry_sign:    | need remap patch         |
| KI2 l1.3    | :no_entry_sign:    | use KI2 l1.3k            |
| KI2 l1.3k   | :white_check_mark: |                          |
| KI2 l1.4    | :no_entry_sign:    | use KI2 l1.4k            |
| KI2 l1.4p   | :no_entry_sign:    | unofficial rom           |
| KI2 d1.4p   | :white_check_mark: |                          |
| KI2 l1.4k   | :white_check_mark: |                          |

### KI2-U96 A-20351

| ROM version | Status             | Note                     |
| ----------- | ------------------ | ------------------------ |
| KI l1.5di   | :white_check_mark: |                          |
| KI l1.5d    | :no_entry_sign:    | use KI l1.5di instead    |
| KI l1.4     | :no_entry_sign:    | need remap+any-ide patch |
| KI l1.3     | :no_entry_sign:    | need remap+any-ide patch |
| KI p47      | :no_entry_sign:    | need remap+any-ide patch |
| KI2 l1.0    | :white_check_mark: |                          |
| KI2 l1.1    | :white_check_mark: |                          |
| KI2 l1.3    | :white_check_mark: |                          |
| KI2 l1.3k   | :no_entry_sign:    | use KI2 l1.3             |
| KI2 l1.4    | :white_check_mark: |                          |
| KI2 l1.4p   | :white_check_mark: |                          |
| KI2 d1.4p   | :no_entry_sign:    | unofficial rom           |
| KI2 l1.4k   | :no_entry_sign:    | unofficial rom           |

- :white_check_mark:: Fully supported
- :no_entry_sign:: Not Supported (requires patches)
- :warning:: Partially Supported (requires full original hardware)

## Features

This boot rom introduces new features compared to the stock boot rom.

- LZSS compression/decompression of game ROM (faster boot time)
- POST (Power On Self Test) (S1:8 to pause boot)
- In-memory patching of game ROM
- Fixes no sound at boot on MAME (if S1:7 is enabled)
- Soft Multiboot K1 & K2 (Requires additional hardware & compilation flag)
- Additional dipswitch configuration bits

## POST

Power On Self Test will check for faulty or missing hardware.

Tested hardware:
- SRAM
- VRAM
- DRAM
- IDE

When S1:8 dipswitch bit is turned on POST result will be kept on screen until any P1 input is triggered.

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

NOTE: This patch is not based and/or related to a patch made by the member of the arcade-projects.com forum @[DogP](https://www.arcade-projects.com/members/dogp.2487/)

### Any IDE

This patch disable the IDE drive model check, allowing to run the game with any IDE compatible drive model.

### Soft reset

This patch adds an input combination allowing to soft reset the game by pressing
P1 LEFT + P1 START + P2 RIGHT + P2 Start.

### Infinite attract mode music

This patch disable the fade out of the music in attract mode (demo mode), allowing the music to play indefinitely.

## DipSwitch S1

| bit | description               | Off | On  |
| --- | ------------------------- | --- | --  |
| 6   | Select default game ROM   | KI1 | KI2 |
| 7   | Disable sound test (Bong) | Off | On  |
| 8   | Wait for input on POST    | Off | On  |

## To do

- Enter infinite loop if a self test failed
- Write any-ide patches for partially supported ROMs
- Write remap patches for non-supported ROMs

## Acknowledgments

- [github.com/DS-Homebrew](https://github.com/DS-Homebrew/nds-bootstrap/blob/master/lzss.c) for the LZSS ROM compression tool
- [github.com/PeterLemon](https://github.com/PeterLemon/N64/blob/master/Compress/LZ77/LZ77Decode/LZ77Decode.asm) for the LZSS mips assembly decompression
- [github.com/azmr](https://github.com/azmr/blit-fonts/blob/master/src/blit32_glyphs.h) for the 5x6 bitmap font
- [arcade-projects.com](https://www.arcade-projects.com/) members for their support and great ideas
- My wife for letting me spending evenings and nights on this projet
