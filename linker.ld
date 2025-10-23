ENTRY(_start)

PHDRS {
  text PT_LOAD FLAGS(5);  /* RX */
  data PT_LOAD FLAGS(6);  /* RW */
}

SECTIONS {
  . = 1M;
  .multiboot_header : { KEEP(*(.multiboot_header)) } :text
  .text : { *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  .data : { *(.data*) } :data
  /* move .bss safely above the page tables */
  . = ALIGN(2M);
  .bss (NOLOAD) : {
      _bss_start = .;
      *(COMMON)
      *(.bss*)
      _bss_end = .;
  } :data

}
