	.text
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
