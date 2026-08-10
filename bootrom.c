/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "bootrom.h"
#include "ramcode.h"
#include "ide.h"
#include "io.h"
#include "wdt.h"
#include "cache.h"
#include "delay.h"
#include "interrupts.h"

#define IDE_STATUS_BSY (0x80)
#define IDE_STATUS_DRDY (0x40)
#define IDE_STATUS_ERR (0x01)

#define IDE_CMD_SELECT_BANK (0xF1)

// The ceiling ide_wait_ready() uses (ide.c:12). The watchdog is kicked every
// iteration, so exhausting it fails the swap instead of rebooting the board.
#define BOOTROM_SPINS (0x5f0000)

RAMCODE_FN bool bootrom_swap(const uint8_t bank)
{
    // The three failure paths below return and must leave IE as they found
    // it, per interrupts.h's contract. The success path never returns -- it
    // jumps into the newly mapped image's reset vector -- so there is
    // nothing to restore there.
    const uint32_t saved = interrupts_disable();

    // Wait for the device to be able to accept a command: !BSY && DRDY. Same
    // test as ide.c:20.
    for (uint32_t spin = BOOTROM_SPINS;
         ((gIDEControl.alternateStatus ^ IDE_STATUS_DRDY) & (IDE_STATUS_BSY | IDE_STATUS_DRDY)) != 0;
         spin--)
    {
        WDT_KICK();
        if (spin == 0)
        {
            interrupts_restore(saved);
            return false;
        }
    }

    gIDE.sectorCount = bank;
    gIDE.command = IDE_CMD_SELECT_BANK;

    // ATA requires 400ns after a command write before the status means
    // anything. ide.c never needed this because it waits on the IDE interrupt.
    udelay(1);

    uint32_t status = 0;
    for (uint32_t spin = BOOTROM_SPINS;; spin--)
    {
        WDT_KICK();
        status = gIDEControl.alternateStatus;
        if ((status & IDE_STATUS_BSY) == 0)
        {
            break;
        }
        if (spin == 0)
        {
            interrupts_restore(saved);
            return false;
        }
    }

    if ((status & IDE_STATUS_ERR) != 0)
    {
        // Aborted: the device has no bank selector. The ROM window still holds
        // our own image, so returning into it is safe.
        interrupts_restore(saved);
        return false;
    }

    /**
     * The ROM window now holds a different image. Nothing below may be fetched
     * from it, and there is no way back.
     *
     * Sweeps all 512 lines of both 16K 2-way primary caches by index, as
     * roms.S:42-51 does: 256 iterations covering way 0 at +0x0 and way 1 at
     * +0x2000. Invalidating the I-cache discards this routine's own remaining
     * instructions; they are re-fetched from SRAM, which did not change.
     */
    for (uint32_t addr = 0x80000000; addr != 0x80002000; addr += CACHE_LINE_SIZE)
    {
        CACHE_OP(INDEX_WRITEBACK_INVALIDATE_D, addr, 0x0000);
        CACHE_OP(INDEX_WRITEBACK_INVALIDATE_D, addr, 0x2000);
        CACHE_OP(INDEX_INVALIDATE_I, addr, 0x0000);
        CACHE_OP(INDEX_INVALIDATE_I, addr, 0x2000);
    }
    SYNC();

    /**
     * jr, not j: j is limited to the 256MB region of the current PC, and the PC
     * is in SRAM at 0x8000xxxx while the reset vector is at 0xBFC00000. The
     * uncached address also means the fetch cannot be served by a cache line
     * the sweep above just invalidated.
     *
     * This enters the *new* image's reset handler, which runs its own
     * la a0,KI_BOOT_VIEW -- how the swapped-in ROM names its own boot view
     * rather than being handed a pointer from an image that no longer exists.
     *
     * The $8 clobber is $t0. Nothing after this executes, but leaving GCC to
     * believe the register survived stops being harmless the moment someone
     * adds a statement above it.
     */
    asm volatile(
        ".set noreorder\n"
        "lui $t0,0xBFC0\n"
        "jr $t0\n"
        "nop\n"
        : : : "$8");

    __builtin_unreachable();
}
