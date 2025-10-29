SECTIONS {
    . = 0x0;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }

    /* Required for -fPIC function calls */
    .plt : { *(.plt*) *(.plt) }
    .got : { *(.got*) *(.got) }
    .got.plt : { *(.got.plt*) *(.got.plt) }
}
