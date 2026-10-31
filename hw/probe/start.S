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

/* The hardware instructions under test. Each is isolated so that the C
 * reference path, built as V7, never contains one. */
#define WRY(r)	wr r, 0, %y; nop; nop; nop

	.globl	hw_sdiv
hw_sdiv:				/* %o0 / %o1, signed, Y = sign of %o0 */
	sra	%o0, 31, %o2
	WRY(%o2)
	retl
	 sdiv	%o0, %o1, %o0

	.globl	hw_udiv
hw_udiv:
	WRY(%g0)
	retl
	 udiv	%o0, %o1, %o0

/* Multiply directly after a load: the sequence Infrant's patches pad with
 * two nops. Arguments are pointers. Result: low word; *hi gets %y. */
	.globl	hw_umul_after_load
hw_umul_after_load:			/* (u32 *a, u32 *b, u32 *hi) */
	ld	[%o0], %o3
	ld	[%o1], %o4
	umul	%o3, %o4, %o0
	rd	%y, %o5
	retl
	 st	%o5, [%o2]

	.globl	hw_umul_padded
hw_umul_padded:
	ld	[%o0], %o3
	ld	[%o1], %o4
	nop
	nop
	umul	%o3, %o4, %o0
	rd	%y, %o5
	retl
	 st	%o5, [%o2]

	.globl	rd_psr
rd_psr:
	retl
	 rd	%psr, %o0

	.globl	rd_tbr
rd_tbr:
	retl
	 rd	%tbr, %o0

	.globl	rd_wim
rd_wim:
	retl
	 rd	%wim, %o0

/* Control register reads through alternate address spaces. */
	.globl	rd_mmu
rd_mmu:
	retl
	 lda	[%o0] 0x10, %o0

	.globl	rd_icmu
rd_icmu:
	retl
	 lda	[%o0] 0x14, %o0

	.globl	rd_dcmu
rd_dcmu:
	retl
	 lda	[%o0] 0x15, %o0
