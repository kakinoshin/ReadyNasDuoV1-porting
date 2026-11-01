/* Privileged register reads, shared by the bare-metal images. */
	.text
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
