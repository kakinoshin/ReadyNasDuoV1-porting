/*
 * MMU fault entry for mmutest.c. Records the trap and the fault registers,
 * then skips the access, so each probe sees a fault as a counter change.
 * Probes are never in a delay slot. A fetch fault has no instruction to skip,
 * so it returns to the caller of the page, whose %o7 is this window's %i7.
 */
#define ASI_MMU		0x10
#define MMU_DFSR	0x10
#define MMU_IFSR	0x14
#define MMU_DFADDR	0x18
#define TT_TEXT_MISS	0x3c
#define TT_TEXT_ACCESS	0x01

	.text
	.globl	mmu_fault
mmu_fault:
	rd	%tbr, %l3
	srl	%l3, 4, %l3
	and	%l3, 0xff, %l3
	set	fault_tt, %l4
	st	%l3, [%l4]
	mov	MMU_DFSR, %l4
	lda	[%l4] ASI_MMU, %l5
	set	fault_dfsr, %l4
	st	%l5, [%l4]
	mov	MMU_IFSR, %l4
	lda	[%l4] ASI_MMU, %l5
	set	fault_ifsr, %l4
	st	%l5, [%l4]
	mov	MMU_DFADDR, %l4
	lda	[%l4] ASI_MMU, %l5
	set	fault_addr, %l4
	st	%l5, [%l4]
	set	fault_count, %l4
	ld	[%l4], %l5
	add	%l5, 1, %l5
	st	%l5, [%l4]

	cmp	%l3, TT_TEXT_MISS
	be	1f
	 cmp	%l3, TT_TEXT_ACCESS
	be	1f
	 nop
	wr	%l0, 0, %psr
	nop
	nop
	nop
	jmp	%l2
	 rett	%l2 + 4
1:
	add	%i7, 8, %l1
	add	%i7, 12, %l2
	wr	%l0, 0, %psr
	nop
	nop
	nop
	jmp	%l1
	 rett	%l2

	.globl	probe_read
probe_read:
	mov	%o0, %o1
	set	0xbadbad00, %o0
	ld	[%o1], %o0
	retl
	 nop

	.globl	probe_write
probe_write:
	st	%o1, [%o0]
	retl
	 nop

	.globl	wr_mmu
wr_mmu:
	sta	%o1, [%o0] 0x10
	retl
	 nop

	.globl	wr_tlb
wr_tlb:
	sta	%o1, [%o0] 0x11
	retl
	 nop
