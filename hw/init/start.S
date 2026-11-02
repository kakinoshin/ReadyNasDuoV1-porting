/* Entry and system call stubs for the test init, without libc. */
	.section .note.netbsd.ident, "a"
	.long	7, 4, 1
	.asciz	"NetBSD"
	.align	4
	.long	1199000000

	.text
	.globl	_start
_start:
	call	main
	 nop
	mov	1, %g1			/* SYS_exit */
	ta	0

#define	SYS(name, nr)			\
	.globl	name;			\
name:					\
	mov	nr, %g1;		\
	ta	0;			\
	bcs,a	1f;			\
	 mov	-1, %o0;		\
1:	retl;				\
	 nop

	SYS(sys_exit, 1)
	SYS(sys_write, 4)
	SYS(sys_open, 5)
	SYS(sys_getpid, 20)
	SYS(sys_wait4, 449)
	SYS(sys_mmap, 197)

/* The child of SYS_fork is told apart by %o1, not %o0. */
	.globl	sys_fork
sys_fork:
	mov	2, %g1
	ta	0
	bcs,a	1f
	 mov	-1, %o0
	tst	%o1
	bne,a	1f
	 clr	%o0
1:	retl
	 nop

	.globl	do_flush
do_flush:
	flush	%o0
	retl
	 nop
