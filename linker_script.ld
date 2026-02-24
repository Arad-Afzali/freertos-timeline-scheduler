/* Linker script for QEMU lm3s6965evb (Cortex-M3) */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 64K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    
    _sidata = LOADADDR(.data);

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        end = .;        /* Symbol required by newlib */
        _end = .;       /* Alternate symbol name */
    } > RAM

    /* Heap section */
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(__heap_start = .);
        . = . + 8K;     /* 8KB heap */
        . = ALIGN(8);
        PROVIDE(__heap_end = .);
    } > RAM

    /* Stack section */
    _estack = ORIGIN(RAM) + LENGTH(RAM);
    
}
