/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "sound.h"
#include "interrupts.h"
#include "delay.h"
#include "io.h"

void sound_write_byte(uint8_t data)
{
    gIO.soundData = data;
    gIO.soundControl = 1;
    udelay(56);
    gIO.soundControl = 3;
    udelay(8);
}

void sound_write_short(uint16_t data)
{
    sound_write_byte(data >> 8);
    sound_write_byte(data & 0xff);
}

void sound_reset_soft(void)
{
    gIO.soundReset = 1;
    udelay(500);
    sound_write_short(0x55aa);
    sound_write_short(0xaa55);
    udelay(1500);
    interrupts_enable();
}

void sound_wait_ready(void)
{
    for (uint32_t i = 0; i < 1500000; i++)
    {
        if ((gIO.soundControl & 0x2) != 0)
        {
            return;
        }
    }
    sound_reset_soft();
}

void sound_set_volume(uint8_t level)
{
    interrupts_disable();
    sound_wait_ready();
    sound_write_short(0x55aa);
    sound_write_short((level << 8) | ~level);
    interrupts_enable();
}

void sound_play(uint16_t track)
{
    interrupts_disable();
    sound_wait_ready();
    sound_write_short(track);
    interrupts_enable();
}

void sound_init(void)
{
    sound_reset_soft();
    sound_set_volume(0xff);
    sound_play(0);
}
