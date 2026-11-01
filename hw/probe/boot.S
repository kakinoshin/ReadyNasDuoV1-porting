/*
 * Entry and trap table for a self-contained IT3107 image. It is entered with
 * traps off and needs nothing from whoever started it except the MMU's
 * kernel direct map.
 *
 * Window traps are handled so that C code can run. Any other trap prints
 * "TRAP <tbr> <pc> <npc>" on the console by polling, then stops.
 */
#define PSR_BOOT	0xfc0		/* PIL 15, S, PS, ET off, CWP 0 */
#define PSR_ET		0x020
#define UART0		0x60000060
#define UART_STAT_TH	0x4
#define NWINDOWS	8
#define PAUSE		nop; nop; nop

#define TRAP(h)		sethi %hi(h), %l0; jmp %l0 + %lo(h); nop; nop
#define BAD		TRAP(bad_trap)

	.text
	.globl	_start
_start:
trapbase:
	b	entry			/* tt 0x00: entry from the loader */
	 nop
	nop
	nop
	.rept	4			/* tt 0x01-0x04 */
	BAD
	.endr
	TRAP(window_overflow)		/* tt 0x05 */
	TRAP(window_underflow)		/* tt 0x06 */
	.rept	249			/* tt 0x07-0xff */
	BAD
	.endr

entry:
	set	PSR_BOOT, %g1
	wr	%g1, 0, %psr
	PAUSE
	mov	2, %g2			/* window 1 is invalid */
	wr	%g2, 0, %wim
	PAUSE
	set	trapbase, %g2
	wr	%g2, 0, %tbr
	PAUSE
	set	stack_top - 96, %sp
	mov	%g0, %fp

	set	__bss_start, %g2
	set	_end, %g3
1:	cmp	%g2, %g3
	bgeu	2f
	 nop
	st	%g0, [%g2]
	b	1b
	 add	%g2, 4, %g2
2:
	wr	%g1, PSR_ET, %psr
	PAUSE
	call	probe_main
	 nop
halt:
	b	halt
	 nop

/* Runs in the invalid window; saves the next one and rotates %wim right. */
window_overflow:
	rd	%wim, %l3
	mov	%g1, %l7
	srl	%l3, 1, %g1
	sll	%l3, NWINDOWS - 1, %l4
	or	%g1, %l4, %g1
	and	%g1, 0xff, %g1
	save
	wr	%g1, 0, %wim
	PAUSE
	std	%l0, [%sp + 0]
	std	%l2, [%sp + 8]
	std	%l4, [%sp + 16]
	std	%l6, [%sp + 24]
	std	%i0, [%sp + 32]
	std	%i2, [%sp + 40]
	std	%i4, [%sp + 48]
	std	%i6, [%sp + 56]
	restore
	mov	%l7, %g1
	jmp	%l1
	 rett	%l2

/* Rotates %wim left and reloads the window being returned to. */
window_underflow:
	rd	%wim, %l3
	sll	%l3, 1, %l4
	srl	%l3, NWINDOWS - 1, %l5
	or	%l5, %l4, %l5
	and	%l5, 0xff, %l5
	wr	%l5, 0, %wim
	PAUSE
	restore
	restore
	ldd	[%sp + 0], %l0
	ldd	[%sp + 8], %l2
	ldd	[%sp + 16], %l4
	ldd	[%sp + 24], %l6
	ldd	[%sp + 32], %i0
	ldd	[%sp + 40], %i2
	ldd	[%sp + 48], %i4
	ldd	[%sp + 56], %i6
	save
	save
	jmp	%l1
	 rett	%l2

/* Traps are off here, so this uses no windows and no stack. */
#define TX_G1				\
9:	ld	[%l3 + 4], %l0;		\
	andcc	%l0, UART_STAT_TH, %g0;	\
	be	9b;			\
	 nop;				\
	st	%g1, [%l3]

#define TX(c)	mov c, %g1; TX_G1

bad_trap:
	set	UART0, %l3
	TX('\r')
	TX('\n')
	TX('T')
	TX('R')
	TX('A')
	TX('P')
	TX(' ')
	rd	%tbr, %l5
	call	hex32
	 nop
	TX(' ')
	mov	%l1, %l5
	call	hex32
	 nop
	TX(' ')
	mov	%l2, %l5
	call	hex32
	 nop
	TX('\r')
	TX('\n')
1:	b	1b
	 nop

/* Print %l5 in hex. Called with traps off; %o7 is this window's scratch. */
hex32:
	mov	8, %l6
1:	srl	%l5, 28, %g1
	sll	%l5, 4, %l5
	cmp	%g1, 10
	bl	2f
	 add	%g1, '0', %g1
	add	%g1, 'a' - '0' - 10, %g1
2:	TX_G1
	subcc	%l6, 1, %l6
	bne	1b
	 nop
	retl
	 nop

	.bss
	.align	8
	.skip	0x4000
stack_top:
