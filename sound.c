/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "sound.h"
#include "interrupts.h"
#include "delay.h"
#include "io.h"
#include "wdt.h"

/*
 * Busy-wait that keeps the watchdog fed. The MAX705 window is ~1.6s, and the
 * boot path between the strobe in start.S and the one in main()'s loop spends
 * far longer than that in here when the DCS does not answer.
 */
static void sound_udelay(uint32_t us)
{
    while (us >= 1000)
    {
        wdt_reset();
        udelay(1000);
        us -= 1000;
    }

    if (us > 0)
    {
        udelay(us);
    }
}

static void sound_reset(void)
{
    const uint32_t irq = interrupts_disable();
    gIO.soundReset = 0;
    udelay(400);
    gIO.soundReset = 1;
    sound_udelay(80000);
    gIO.soundData = 1;
    interrupts_restore(irq);
}

static uint8_t sound_wait_ready(void)
{
    for (uint32_t i = 0; i < 1500000; i++)
    {
        if ((gIO.soundControl & 0x2) != 0)
        {
            return 1;
        }

        // Each iteration is an uncached read, so a full timeout runs for
        // hundreds of milliseconds; without this the watchdog fires first and
        // the timeout can never be reached.
        if ((i & 0xFFF) == 0)
        {
            wdt_reset();
        }
    }
    return 0;
}

static void sound_write_byte(uint8_t data)
{
    gIO.soundData = data;
    gIO.soundControl = 1;
    udelay(56);
    gIO.soundControl = 3;
    udelay(8);
}

static void sound_volume_command(uint16_t cmd)
{
    const uint32_t irq = interrupts_disable();
    uint32_t data = ((uint32_t)cmd << 16) | 0x55AA;

    do
    {
        if (!sound_wait_ready())
        {
            sound_reset();
            interrupts_restore(irq);
            return;
        }

        udelay(8);
        sound_write_byte((uint8_t)((data >> 8) & 0xFF));

        if (!sound_wait_ready())
        {
            sound_reset();
            interrupts_restore(irq);
            return;
        }

        udelay(8);
        sound_write_byte((uint8_t)(data & 0xFF));
        data >>= 16;
    } while (data != 0);

    interrupts_restore(irq);
}

void sound_play(uint16_t track)
{
    const uint32_t irq = interrupts_disable();

    if (!sound_wait_ready())
    {
        sound_reset();
        interrupts_restore(irq);
        return;
    }

    sound_write_byte((uint8_t)(track >> 8));

    if (!sound_wait_ready())
    {
        sound_reset();
        interrupts_restore(irq);
        return;
    }

    udelay(8);
    sound_write_byte((uint8_t)(track & 0xFF));
    interrupts_restore(irq);
}

void sound_set_volume(uint8_t level)
{
    sound_volume_command(((uint16_t)level << 8) | (uint8_t)~level);
}

void sound_init(void)
{
    sound_reset();          // Reset DCS
    sound_write_byte(0x00); // Send a byte to force boot, skip self-test
    sound_udelay(100000);   // Wait a least 100ms
    sound_set_volume(0x80); // Set volume to max
}
