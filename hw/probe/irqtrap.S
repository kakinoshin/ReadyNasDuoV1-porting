/*
 * Interrupt entry for irqtest.c. Stays in the trap window and changes no
 * condition codes. Records the trap type and cause, masks and clears
 * everything, then restores only the mask in irq_rearm_h, so a forced
 * interrupt fires once and the timer keeps running.
 */
#define INTC		0x60000000
#define INTC_MASK_H	0x00
#define INTC_MASK_L	0x04
#define INTC_FORCE_H	0x10
#define INTC_FORCE_L	0x14
#define INTC_CLR_H	0x18
#define INTC_CLR_L	0x1c

	.text
	.globl	irq_handler
irq_handler:
	rd	%tbr, %l3
	srl	%l3, 4, %l3
	and	%l3, 0xff, %l3
	set	irq_last_tt, %l4
	st	%l3, [%l4]

	set	INTC, %l4
	ld	[%l4 + INTC_CLR_H], %l5
	ld	[%l4 + INTC_CLR_L], %l6
	set	irq_cause_h, %l7
	st	%l5, [%l7]
	set	irq_cause_l, %l7
	st	%l6, [%l7]

	st	%g0, [%l4 + INTC_MASK_H]
	st	%g0, [%l4 + INTC_MASK_L]
	st	%g0, [%l4 + INTC_FORCE_H]
	st	%g0, [%l4 + INTC_FORCE_L]
	st	%l5, [%l4 + INTC_CLR_H]
	st	%l6, [%l4 + INTC_CLR_L]

	set	irq_rearm_h, %l7
	ld	[%l7], %l5
	st	%l5, [%l4 + INTC_MASK_H]
	set	0x66660000, %l7
	st	%g0, [%l7]

	set	irq_count, %l7
	ld	[%l7], %l5
	add	%l5, 1, %l5
	st	%l5, [%l7]

	jmp	%l1
	 rett	%l2

	.globl	wr_psr
wr_psr:
	wr	%o0, 0, %psr
	nop
	nop
	nop
	retl
	 nop
