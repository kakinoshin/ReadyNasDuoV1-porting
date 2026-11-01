/* Traditional Linux/sparc text base. */
OUTPUT_FORMAT("elf32-sparc")
OUTPUT_ARCH(sparc)
ENTRY(_start)
SECTIONS
{
	. = 0x10000 + SIZEOF_HEADERS;
	.text : { *(.text.startup) *(.text*) }
	.rodata : { *(.rodata*) }
	. = ALIGN(0x4000);
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
