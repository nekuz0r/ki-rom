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

    # The stack grows down from the top of the first 16K of cached SRAM towards
    # _ebss. This is deliberately a bare symbol rather than an output section:
    # as a section it spanned 0x80000000-0x80004000 and therefore overlapped
    # .data and .bss, which ld reports as a hard error under any non-binary
    # output format. OUTPUT_FORMAT(binary) merely suppressed the diagnostic.
    _stack_vma = ORIGIN(sram) + 0x4000; # Base address for the stack (growing down)

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

    # .data + .bss grow up from 0x80000200 into the region the stack grows down
    # through, and nothing else bounds them. Reserve 8K: the detour gateway
    # stacks sit at _stack_vma - 0x1000 and below (see detour.h).
    ASSERT(_ebss <= _stack_vma - 0x2000, "ERROR: .data + .bss leave less than 8K for the stack")
    ASSERT(_heap_vma + _heap_size <= 0x80030000, "ERROR: heap overruns VRAM bank 0")
    ASSERT(SIZEOF(.text) + SIZEOF(.data) <= LENGTH(rom), "ERROR: image does not fit in ROM")

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
