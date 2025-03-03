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

    .data ORIGIN(sram) + 0x200 : ALIGN(8) {
        _sdata = .;
        *(.sdata);
        *(.data);
        *(.rodata);
        *(.rodata.*);
        *(.detour.*);
        _edata = .;
    } > sram AT > rom

    .bss ORIGIN(sram) + 0x200 + SIZEOF(.data) (NOLOAD) : ALIGN(8) {
        _sbss = .;
        *(.sbss);
        *(.bss);
        *(COMMON);
        _ebss = .;
    } > sram

    # In this configuration the stack and heap do grow in opposide direction and away of each other
    .heap ORIGIN(sram) + 0x4000 (NOLOAD) : ALIGN(8) {
        _heap_vma = .; # Base address for the heap (growing up)
        . += 0x10000;
    } > sram

    .stack ORIGIN(sram) (NOLOAD) : ALIGN(8) {
        . += 0x4000; # Put the stack at the edge of the 16K of cached memory
        _stack_vma = .; # Base address for the stack (growing down)
    } > sram

    .fill : {
        LONG(0x0);
        FILL(0x00);
        . = ORIGIN(rom) + LENGTH(rom);
    } > rom

    _gp = _sdata + 0x7ff0;
    
    PROVIDE(_sdata_vma = ADDR(.data));
    PROVIDE(_sdata_lma = LOADADDR(.data));
    PROVIDE(_data_size = SIZEOF(.data));
    PROVIDE(_sbss_vma = ADDR(.bss));
    PROVIDE(_bss_size = SIZEOF(.bss));
    PROVIDE(_heap_size = SIZEOF(.heap));

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
