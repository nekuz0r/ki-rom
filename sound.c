/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "sound.h"
#include "interrupts.h"
#include "delay.h"
#include "io.h"

static void sound_reset(void)
{
    gIO.soundReset = 0;
    udelay(400);
    gIO.soundReset = 1;
    delay(80);
    gIO.soundData = 1;
    interrupts_enable();
}

static uint8_t sound_wait_ready(void)
{
    for (uint32_t i = 0; i < 1500000; i++)
    {
        if ((gIO.soundControl & 0x2) != 0)
        {
            return 1;
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

static void sound_command(uint16_t cmd)
{
    interrupts_disable();
    uint32_t data = ((uint32_t)cmd << 16) | 0x55AA;

    do
    {
        if (!sound_wait_ready())
        {
            sound_reset();
            return;
        }

        udelay(8);
        sound_write_byte((uint8_t)(data & 0xFF));
        data >>= 8;
    } while (data != 0);

    interrupts_enable();
}

void sound_write_short(uint16_t data)
{
    sound_write_byte(data >> 8);
    sound_write_byte(data & 0xff);
}

void sound_play(uint16_t track)
{
    interrupts_disable();

    if (!sound_wait_ready())
    {
        sound_reset();
        return;
    }

    sound_write_byte((uint8_t)(track >> 8));

    if (!sound_wait_ready())
    {
        sound_reset();
        return;
    }

    udelay(8);
    sound_write_byte((uint8_t)(track & 0xFF));
    interrupts_enable();
}

void sound_set_volume(uint8_t level)
{
    sound_command((level << 8) | ~level);
}

void sound_init(void)
{
    sound_reset();
    sound_set_volume(0xff);
    sound_play(0);
}
