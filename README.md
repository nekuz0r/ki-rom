# Killer instinct Boot ROM

This project is an open source Boot ROM for Killer instinct 1 & 2 arcade game developed by Rare and published by Midway.

## Compatibility

### KI-U96 A-19489

| ROM version | Status             | Note                     |
| ----------- | ------------------ | ------------------------ |
| KI l1.5d    | :white_check_mark: |                          |
| KI l1.5di   | :white_check_mark: | unofficial rom           |
| KI l1.4     | :white_check_mark: |                          |
| KI l1.3     | :white_check_mark: |                          |
| KI p47      | :warning:          | need any-ide patch       |
| KI2 l1.0    | :white_check_mark: |                          |
| KI2 l1.1    | :white_check_mark: |                          |
| KI2 l1.3    | :no_entry_sign:    | use KI2 l1.3k            |
| KI2 l1.3k   | :white_check_mark: |                          |
| KI2 l1.4    | :no_entry_sign:    | use KI2 l1.4k            |
| KI2 l1.4k   | :white_check_mark: |                          |
| KI2 l1.4p   | :no_entry_sign:    | unofficial rom           |
| KI2 d1.4p   | :white_check_mark: | unofficial rom           |

### KI2-U96 A-20351

| ROM version | Status             | Note                     |
| ----------- | ------------------ | ------------------------ |
| KI l1.5di   | :white_check_mark: | unofficial rom           |
| KI l1.5d    | :white_check_mark: |                          |
| KI l1.4     | :white_check_mark: |                          |
| KI l1.3     | :white_check_mark: |                          |
| KI p47      | :no_entry_sign:    | need remap patch         |
| KI2 l1.0    | :white_check_mark: |                          |
| KI2 l1.1    | :white_check_mark: |                          |
| KI2 l1.3    | :white_check_mark: |                          |
| KI2 l1.3k   | :no_entry_sign:    | use KI2 l1.3             |
| KI2 l1.4    | :white_check_mark: |                          |
| KI2 l1.4k   | :no_entry_sign:    | use KI2 l1.4             |
| KI2 l1.4p   | :white_check_mark: | unofficial rom           |
| KI2 d1.4p   | :no_entry_sign:    | unofficial rom           |

- :white_check_mark:: Fully supported
- :no_entry_sign:: Not Supported (requires patches)
- :warning:: Partially Supported (requires full original hardware)

## Features

This boot rom introduces new features compared to the stock boot rom.

- LZSS compression/decompression of game ROM (faster boot time)
- In-memory patching of game ROM
- Fixes no sound at boot on MAME
- Replaced "Bong" boot sound
- Pause on boot
- Soft Multiboot K1 & K2 (Requires additional hardware & compilation flag)
- Additional dipswitch configuration bits

## POST

Power On Self Test will check for faulty or missing hardware.

Tested hardware:
- SRAM
- VRAM
- DRAM
- IDE

When S1:7 dipswitch bit is turned on POST result will be kept on screen until any P1 input is triggered.

**NOTE: This has been moved to a dedicated board testing ROM**

## Patches

| ROM version | I/O Remap          | A-20383 Bypass     | AnyIDE             | 2In1 HDD           | Reset              | No Music Fade out  | No Whiteblood      |
| ----------- | ------------------ | ------------------ | ------------------ | ------------------ | ------------------ | ------------------ | ------------------ | 
| KI l1.5d    | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| KI l1.5di   | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| KI l1.4     | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI l1.3     | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.4    |                    |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.4k   |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.3    |                    |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.3k   |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.1    | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |
| KI2 l1.0    | :white_check_mark: |                    | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |                    |

### KI2-U96 A-20351 remap

This patch remaps I/O memory addresses to allow KI1 ROM to run on KI2 dedicated hardware and vice versa.

### K12-U1 A-20383 protection bypass

This patch disables the copy protection of the KI1 to KI2 upgrade kit.

NOTE: This patch is not based and/or related to a patch made by the member of the arcade-projects.com forum @[DogP](https://www.arcade-projects.com/members/dogp.2487/)

### Any IDE

This patch disables the IDE drive model check, allowing to run the game with any IDE compatible drive model.

### Soft reset

This patch adds an input combination allowing to soft reset the game by pressing
P1 UP + P1 START + P1 FP.

### Infinite attract mode music

This patch disables the fade out of the music in attract mode (demo mode), allowing the music to play indefinitely.

## DipSwitch S1

| bit | description               | Off    | On        |
| --- | ------------------------- | ------ | --------- |
| 6   | Disable boot sounds       | Sounds | No sounds |
| 7   | Wait for input on boot    | Wait   | Continue  |

## To do

- Write any-ide patches (KI1 p47)
- Write remap patches (KI1 p47)

## Acknowledgments

- [github.com/DS-Homebrew](https://github.com/DS-Homebrew/nds-bootstrap/blob/master/lzss.c) for the LZSS ROM compression tool
- [github.com/PeterLemon](https://github.com/PeterLemon/N64/blob/master/Compress/LZ77/LZ77Decode/LZ77Decode.asm) for the LZSS mips assembly decompression
- [arcade-projects.com](https://www.arcade-projects.com/) members for their support and great ideas
- My wife for letting me spending evenings and nights on this projet
