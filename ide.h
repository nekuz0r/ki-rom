/**
 * SPDX-FileCopyrightText: © 2023 Leandre Gohy <leandre.gohy@hexeo.be>
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#ifndef _IDE_H_
#define _IDE_H_

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    _Alignas(8) uint32_t data; // offset = 0x100 (rw)
    _Alignas(8) union
    {
        uint32_t error;    // offset = 0x108 (r)
        uint32_t features; // offset = 0x108 (w)
    };
    _Alignas(8) uint32_t sectorCount; // offset = 0x110 (rw)
    _Alignas(8) uint32_t lbaLow;      // offset = 0x118 (rw)
    _Alignas(8) uint32_t lbaMid;      // offset = 0x120 (rw)
    _Alignas(8) uint32_t lbaHigh;     // offset = 0x128 (rw)
    _Alignas(8) uint32_t device;      // offset = 0x130 (rw)
    _Alignas(8) union
    {
        uint32_t status;  // offset = 0x138 (r)
        uint32_t command; // offset = 0x138 (w)
    };
} ide_command_registers_t;

// TEST: IDE Command offsets
static_assert(0x100 + offsetof(ide_command_registers_t, data) == 0x100);
static_assert(0x100 + offsetof(ide_command_registers_t, error) == 0x108);
static_assert(0x100 + offsetof(ide_command_registers_t, features) == 0x108);
static_assert(0x100 + offsetof(ide_command_registers_t, sectorCount) == 0x110);
static_assert(0x100 + offsetof(ide_command_registers_t, lbaLow) == 0x118);
static_assert(0x100 + offsetof(ide_command_registers_t, lbaMid) == 0x120);
static_assert(0x100 + offsetof(ide_command_registers_t, lbaHigh) == 0x128);
static_assert(0x100 + offsetof(ide_command_registers_t, device) == 0x130);
static_assert(0x100 + offsetof(ide_command_registers_t, status) == 0x138);
static_assert(0x100 + offsetof(ide_command_registers_t, command) == 0x138);

typedef struct
{
    _Alignas(8) union
    {
        uint32_t alternateStatus;
        uint32_t deviceControl;
    };
} ide_control_registers_t;

// TEST: IDE Control offsets
static_assert(0x170 + offsetof(ide_control_registers_t, alternateStatus) == 0x170);
static_assert(0x170 + offsetof(ide_control_registers_t, deviceControl) == 0x170);

extern volatile ide_command_registers_t gIDE;
extern volatile ide_control_registers_t gIDEControl;

uint8_t ide_init(void);
void ide_read_sectors(uint32_t lba, uint32_t count, void *buf);
void ide_write_sectors(uint32_t lba, uint32_t count, void *buf);

#endif
