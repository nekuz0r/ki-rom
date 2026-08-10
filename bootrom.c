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
#define IDE_STATUS_DRQ (0x08)
#define IDE_STATUS_ERR (0x01)

#define IDE_CMD_SELECT_BANK (0xF1)

// The ceiling ide_wait_ready() uses (ide.c:12). The watchdog is kicked every
// iteration, so exhausting it fails the swap instead of rebooting the board.
#define BOOTROM_SPINS (0x5f0000)

RAMCODE_FN void bootrom_swap(const uint8_t bank)
{
    // The two paths below that return -- pre-command timeout and an aborted
    // command -- must leave IE as they found it, per interrupts.h's
    // contract. Success and a post-command timeout both end by jumping into
    // a reset vector instead of returning, so neither restores.
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
            // No command has been issued yet, so the bank is still whatever
            // it was on entry: returning into this image is safe.
            interrupts_restore(saved);
            return;
        }
    }

    gIDE.sectorCount = bank;
    gIDE.command = IDE_CMD_SELECT_BANK;

    // ATA requires 400ns after a command write before the status means
    // anything. ide.c never needed this because it waits on the IDE interrupt.
    udelay(1);

    bool timed_out = false;
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
            // The command was already issued, so the bank may already have
            // flipped or be mid-flip: returning here would execute whatever
            // bytes now sit at the caller's ROM address in a different
            // image. Reset instead, below -- it is correct whichever bank
            // ended up mapped, because both images share the reset vector
            // layout.
            timed_out = true;
            break;
        }
    }

    // 0xF1 is not a free vendor opcode: from ATA-3 on it is SECURITY SET
    // PASSWORD, a PIO data-out command. A drive with no bank selector but
    // that does implement Security clears BSY with DRQ set and ERR clear,
    // waiting for a 512-byte password block that this code never sends. A
    // test that looked only at ERR would read that as a completed swap and
    // jump into a bank that never moved. DRQ never sets on switcher hardware
    // -- the switcher consumes 0xF1 before any drive sees it -- so testing it
    // here costs nothing there and is the only thing standing between this
    // command and SECURITY SET PASSWORD everywhere else.
    if (!timed_out && (status & (IDE_STATUS_ERR | IDE_STATUS_DRQ)) != 0)
    {
        // Aborted, or stuck waiting for data that is never coming: either way
        // the device did not switch banks, so the ROM window still holds our
        // own image and returning into it is safe.
        //
        // That safety is only as good as the assumption that the switcher
        // *consumes* 0xF1 rather than snooping it -- a device that answers
        // here is a device the command reached, and on this hardware that
        // only happens when nothing switched the bank. A switcher that
        // instead snooped the bus, flipping the bank while also letting an
        // attached drive answer, would break this: the bank could already be
        // wrong by the time this path returns, and the caller would resume
        // executing whatever now sits at its own ROM address in a different
        // image -- a hang with no output and nothing to say why.
        //
        // The command still raised INTRQ, and alternateStatus above is
        // defined not to clear it -- only a read of the real Status register
        // does. Left unacknowledged, the next command that waits on Cause's
        // IP bit for its own completion would see this stale assertion and
        // return immediately, before its data is ready. ide_ack() (ide.c:24)
        // always ends on this same read for exactly that reason; this
        // command has no ide_ack() call of its own, so it is done here by
        // hand.
        (void)gIDE.status;
        interrupts_restore(saved);
        return;
    }

    /**
     * Reached on success, or on a post-command timeout whose outcome is
     * unknown. Either way the ROM window may now hold a different image, and
     * there is no way back: nothing below may be fetched from it.
     *
     * Unlike the ERR path above, INTRQ is left unacknowledged here -- there is
     * no ide_ack() call and no read of gIDE.status. That is safe only because
     * whichever image now owns the reset vector runs ide_init() (ide.c:48)
     * before it does anything else with the device, and ide_init() opens by
     * asserting SRST, which clears a pending interrupt along with every other
     * piece of state a stale INTRQ could poison.
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
     * is in SRAM at 0x8000xxxx while the reset vector is at 0xBFC00000. 0xBFC0
     * is kseg1, so the fetch bypasses the I-cache entirely -- the jump lands
     * correctly even if the sweep above were wrong.
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
