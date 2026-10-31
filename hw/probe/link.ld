/* iboot's tftp/go load address for custom binaries. */
OUTPUT_FORMAT("elf32-sparc")
OUTPUT_ARCH(sparc)
ENTRY(_start)
SECTIONS
{
	. = 0x80040000;
	.text : { *(.text.startup) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	. = ALIGN(4);
	__bss_start = .;
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(4);
	_end = .;
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
