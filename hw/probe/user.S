/*
 * Linux/sparc entry and system calls for the userland probe. There is no
 * libc: a current glibc does not run on the RAIDiator 2.6.17 kernel.
 */
#define SYS_exit	1
#define SYS_write	4

	.text
	.globl	_start
_start:
	mov	%g0, %fp
	sub	%sp, 96, %sp
	call	probe_main
	 nop
	mov	0, %o0
	mov	SYS_exit, %g1
	ta	0x10

	.globl	sys_write
sys_write:
	mov	SYS_write, %g1
	ta	0x10
	retl
	 nop
