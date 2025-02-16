/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "ide.h"
#include "wdt.h"
#include "delay.h"

uint16_t ide_wait_ready(void)
{
    uint32_t count = 0x5f0000;
    do
    {
        if (count == 0)
        {
            return 0x100; // Indicate a timeout condition
        }
        count--;
    } while (((gIDEControl.alternateStatus ^ 0x40) & 0xc0) != 0); // Device is not ready or busy
    return gIDEControl.alternateStatus;
}

uint16_t ide_ack(void)
{
    uint32_t count = 0x2460000;
    register uint32_t cause;
    do
    {
        if (count == 0)
        {
            return 0x100; // Indicate a timeout condition
        }
        count--;

        asm volatile("mfc0 %0,$13"
                     : "=r"(cause));
    } while ((cause & 0x800) == 0);
    return gIDE.status;
}

uint8_t ide_init(void)
{
    /**
     * We force the device to be put in soft reset.
     * This is done to wipe any previous configuration (like soft reset from the game)
     */
    gIDEControl.deviceControl = 0xC; // Put device in Soft reset state
    delay(10);                       // Datasheet says 5ms before exiting reset state
    gIDEControl.deviceControl = 0x8; // Disable reset (!SRST) & Interrupt enabled (!IEN)

    ide_wait_ready();
    gIDE.sectorCount = 0x28; // Number of sector per track
    gIDE.device = 0xd;       // Number of head - 1 per cylinder
    gIDE.command = 0x91;     // Initialize device parameters
    return ide_ack() & 0x21; // Return DRDY and ERR bits
}

void ide_seek(uint32_t lba, uint8_t count)
{
    gIDE.sectorCount = count;                  // Sector count
    gIDE.lbaLow = (lba % 0x28) + 1;            // Sector
    gIDE.lbaMid = ((lba / 0x28) / 0xe) & 0xff; // Cylinder (LOW)
    gIDE.lbaHigh = ((lba / 0x28) / 0xe) >> 8;  // Cylinder (HI)
    gIDE.device = (lba / 0x28) % 0xe;          // Head
}

void ide_read_sector_bytes(uint16_t *ptr)
{
    uint16_t *end = ptr + 0x100;
    do
    {
        *ptr++ = gIDE.data;
    } while (ptr != end);
}

void ide_read_sectors(uint32_t lba, uint32_t count, void *buf)
{
    if (count <= 0)
    {
        return;
    }

    // Only 255 sectors can be requested at a time
    uint16_t sector_count = count & 0xff;
    do
    {
        wdt_reset();
        count -= sector_count;
        ide_wait_ready();
        ide_seek(lba, sector_count & 0xff);
        gIDE.command = 0x20; // Read sectors command

        // Read sectors
        do
        {
            ide_ack();
            ide_read_sector_bytes(buf);
            buf += 0x200;
            lba++;
            sector_count--;
        } while (sector_count > 0);
        sector_count = 0x100; // 256 sectors will be transfered
    } while (count > 0);
}

void ide_write_sector_bytes(uint16_t *ptr)
{
    uint16_t *end = ptr + 0x100;
    do
    {
        gIDE.data = *ptr++;
    } while (ptr != end);
}

void ide_write_sectors(uint32_t lba, uint32_t count, void *buf)
{
    if (count <= 0)
    {
        return;
    }

    // Only 255 sectors can be requested at a time
    uint16_t sector_count = count & 0xff;
    do
    {
        wdt_reset();
        count -= sector_count;
        ide_wait_ready();
        ide_seek(lba, sector_count & 0xff);
        gIDE.command = 0x31; // write sectors command

        // write sectors
        do
        {
            ide_write_sector_bytes(buf);
            ide_ack();
            buf += 0x200;
            lba++;
            sector_count--;
        } while (sector_count > 0);
        sector_count = 0x100; // 256 sectors will be transfered
    } while (count > 0);
}
