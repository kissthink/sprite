/*
 * gstart.s --
 *
 *	Profiling crt0 for sun4.  This is the header that sets up argc,
 *      argv and envp for main() and then jumps there.  It also initializes
 *      the values of certain registers.  The profiling routines are
 *      initialized.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <kernel/machConst.h>
.seg	"data"
.asciz	"$Header: /sprite/src/lib/c/crt/sun4.md/RCS/gstart.s,v 1.2 91/11/11 15:56:50 jhh Exp $"

.align	8
.seg	"text"

.globl	gstart

/*
 * The stack pointer should be double-word aligned when it gets here.  At
 * MACH_FULL_STACK_FRAME beneath it lies the user stack put together in
 * DoExec().  At the top of this is the argc integer.  Below that is the argv
 * stuff and below that is the
 * environment stuff.
 */
gstart:
	/* Mark last stack frame so debugger can tell bottom of stack. */
	clr	%fp

/*----------------------------------------------------------------------*/

        /*
	 *  Special stuff for dealing with profiling.  Except for this
	 *  stuff, this file is the same as start.s
	 */

	sethi   %hi(0x2000), %o0        /* 0x2000 = start of text segment   */
        sethi   %hi(_etext), %o1
	call    _monstartup             /* monstartup(etext, 0x2000)        */
	or      %lo(_etext), %o1, %o1

        sethi   %hi(__mcleanup), %o0
	call    _atexit                         /* atexit(_mcleanup) */
	or      %lo(__mcleanup), %o0, %o0

	sethi   %hi(_errno),%o0
	st	%g0,[%o0+%lo(_errno)]           /* errno = 0 */

/*----------------------------------------------------------------------*/

	/*
	 * Argc is the first word on the top of the stack (top == lowest addr).
	 * This is followed immediately by the array of pointers to arguments
	 * (argv) and this is followed immediately by the array of pointers to
	 * the environment (envp).  We must calculate the address of envp by
	 * using the number of arguments given in argc.
	 * environ = (char *) argv + (argc + 1) * 4.
	 * And argv = top of stack plus sizeof (Address).
	 */
	mov	%sp, %VOL_TEMP1
	add	%VOL_TEMP1, MACH_FULL_STACK_FRAME, %VOL_TEMP1
	ld	[%VOL_TEMP1], %o0		/* argc */
	add	%VOL_TEMP1, 4, %o1		/* argv pointer */
	add	%o0, 1, %o2
	sll	%o2, 2, %o2
	add	%o2, %o1, %o2		/* envp pointer */
	set	_environ, %o3
	st	%o2, [%o3]		/* store addr of env into environ */

	/* Off to main routine with arguments argc, argv, envp */
	call	_main, 3
	nop
	call	_exit
	nop

