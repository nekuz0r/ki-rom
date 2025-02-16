OUTPUT_FORMAT(binary)

ENTRY(_reset_exception_handler)

MEMORY {
    rom (rx) : ORIGIN = 0x9fc00000, LENGTH = 512K
    sram (rw) : ORIGIN = 0x80000000, LENGTH = 512K
    dram (rwx) : ORIGIN = 0x88000000, LENGTH = 8M
}

SECTIONS {
    .text : ALIGN(4) {
        *(.text);
        *(.assets);
    } > rom AT > rom

    .data ORIGIN(sram) : ALIGN(8) {
        _sdata = .;
        *(.sdata);
        *(.data);
        *(.rodata);
        *(.rodata.*);
        *(.detour.*);
        _edata = .;
    } > sram AT > rom

    .bss (NOLOAD) : ALIGN(8) {
        _sbss = .;
        *(.sbss);
        *(.bss);
        *(COMMON);
        . += 0x2800;
        gAssetBuffer = .;
        _ebss = .;
    } > sram

    .fill : {
        LONG(0x0);
        FILL(0x00);
        . = ORIGIN(rom) + LENGTH(rom);
    } > rom

    _gp = _sdata + 0x7ff0;
    
    PROVIDE(_stack_vma = ORIGIN(sram) + SIZEOF(.data) + SIZEOF(.bss) + 0x2000);
    PROVIDE(_sram_user = _ebss);
    PROVIDE(_sdata_vma = ADDR(.data));
    PROVIDE(_sdata_lma = LOADADDR(.data));
    PROVIDE(_data_size = SIZEOF(.data));
    PROVIDE(_sbss_lma = _sbss);
    PROVIDE(_bss_size = SIZEOF(.bss));

    PROVIDE(gIO = 0xB0000080);
    PROVIDE(gIDE = 0xB0000100);
    PROVIDE(gIDEControl = 0xB0000170);
    PROVIDE(gVramBank0 = 0x80030000);
    PROVIDE(gVramBank1 = 0x80058000);

    /DISCARD/ : { 
        *(.reginfo)
        *(.MIPS.abiflags)
        *(.rel.*)
        *(.pdr)
        *(.shstrtab)
        *(.strtab)
        *(.symtab)
        *(.gnu.*)
        *(.note.*)
        *(.comment)
        *(.mdebug.*)
    }
}
