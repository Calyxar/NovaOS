ENTRY(_start)

SECTIONS {
    . = 0x100000;

    .multiboot ALIGN(4) : { *(.multiboot) }
    .text      ALIGN(4K) : { *(.text) }
    .rodata    ALIGN(4K) : { *(.rodata) }
    .data      ALIGN(4K) : { *(.data) }
    .bss       ALIGN(4K) : {
        kernel_bss_start = .;
        *(COMMON)
        *(.bss)
        kernel_bss_end = .;
    }
    kernel_end = .;
}
