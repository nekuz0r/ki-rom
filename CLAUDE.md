# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Open-source boot ROM for Killer Instinct 1 & 2 arcade machines (Rare/Midway). Bare-metal MIPS R4600 firmware that boots the game from IDE hard drive, applies runtime patches, and provides a menu UI. Runs on two PCB revisions with different I/O layouts.

## Build Commands

```bash
# Build a single ROM (default: BOARD=19489 ROM=ki-l15d)
make rom BOARD=19489 ROM=ki-l15d

# Build all 16 ROM variants for both boards
make roms

# Clean build artifacts
make clean

# Build HDD image
make hdd

# Copy to MAME directory
make mame BOARD=19489 ROM=ki-l15d
```

**Toolchain:** `mipsel-linux-gnu-gcc-14` (MIPS little-endian cross-compiler). Requires `libpng-dev` for host tools. Docker build available via `Dockerfile`.

**Output:** `output/BOARD-ROM.u98` (raw 512K binary), `output/BOARD-ROM.txt` (disassembly).

**Build variables:** `BOARD` (19489 or 20351), `ROM` (e.g., ki-l15d, ki2-l14k). These set preprocessor defines `KI_BOARD_19489`/`KI_BOARD_20351` and ROM version identifiers.

## Architecture

### Target Hardware
- **CPU:** MIPS R4600 @ 100MHz pipeline (50MHz input x2), little-endian, o64 ABI
- **Board A-19489:** KI1 original board (runs both KI1 and KI2)
- **Board A-20351:** KI2 dedicated board (different I/O register layout)

### Memory Map (from boot.ld)
- `0x9FC00000` - ROM (512K, read-only, execution starts here)
- `0x80000000` - SRAM (512K): stack at 0x0-0x4000, heap at 0x4000-0x14000, VRAM banks at 0x30000/0x58000
- `0x88000000` - DRAM (8MB, game ROM loaded here)
- `0xB0000080` - GPIO registers (`gIO`): buttons, sound, video control
- `0xB0000100` - IDE interface (`gIDE`)

### Boot Flow
`start.S` (exception vectors, CPU/cache init, segment copy) -> `main.c` (video/sound/timer init, view render loop)

### Key Abstractions

**View system** (`view.h`): Screen abstraction with `render()`, `load()`, `unload()` callbacks. Views: `view_main` (boot screen), `view_bootselect` (ROM picker).

**Function detour system** (`detour.h`/`detour.c`): Runtime patching of game ROM functions. Copies original instructions to a gateway buffer, replaces with jump to hook. Uses a separate stack (0x1000 bytes below main stack) to avoid corruption from interrupt-time detours. Requires explicit I-cache invalidation after patching.

**Board-conditional I/O** (`io.h`): `gpio_t` struct layout differs between boards via `#if defined(KI_BOARD_19489)` / `KI_BOARD_20351`. Same field names (`soundControl`, `soundReset`, `soundData`, etc.) at different offsets. Hardware accessed through `extern volatile gpio_t gIO` (linker-provided at `0xB0000080`).

**Patches** (`patch_*.c`): Each file implements game ROM modifications using the detour system. Applied at boot based on compile-time ROM/board selection.

### Conventions
- C23 standard (`-std=gnu23`), freestanding (no libc beyond headers)
- `g` prefix for global hardware symbols (`gIO`, `gIDE`, `gVramBank0`)
- `_s`/`_e` prefix pairs for linker segment boundaries (`_sdata`/`_edata`, `_sbss`/`_ebss`)
- Inline assembly with `volatile` for all hardware register access and CP0 operations
- `DETOUR_FN` attribute macro for hook functions (placed in `.detour.hook` section)
- `udelay(us)` / `delay(ms)` are always-inline busy-wait loops calibrated to 25 iterations = 1us
- Interrupts explicitly disabled/enabled around sound hardware access and other critical sections
- Assets compressed with LZSS at build time, decompressed in MIPS assembly at runtime (`lzss.S`)

### Reverse Engineering Context
ROM images in `assets/roms/` are analyzed with Ghidra (MIPS:LE:64:64-32addr, n32 compiler spec). The project file is `ki-l15di-game-rom.bin`. Ghidra's decompiler can miss instructions in MIPS code due to unreachable block elimination — always verify against raw disassembly for critical functions.
