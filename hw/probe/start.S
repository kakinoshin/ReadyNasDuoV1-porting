/*
 * Entry stub for the IT3107 bare-metal probe. iboot's `go` jumps here with
 * its own stack and trap table, which we keep: nothing here may trap.
 * Returning goes back to iboot.
 */
	.text
	.globl	_start
_start:
	save	%sp, -96, %sp

	/* Clear .bss by hand; the probe uses no libgcc memset. */
	sethi	%hi(__bss_start), %o0
	or	%o0, %lo(__bss_start), %o0
	sethi	%hi(_end), %o1
	or	%o1, %lo(_end), %o1
1:	cmp	%o0, %o1
	bgeu	2f
	 nop
	st	%g0, [%o0]
	b	1b
	 add	%o0, 4, %o0
2:
	call	probe_main
	 nop
	ret
	 restore
