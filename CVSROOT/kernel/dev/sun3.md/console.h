/*
 * console.h --
 *
 *	Declarations for things exported by devConsole.c to the rest
 *	of the device module.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header$ SPRITE (Berkeley)
 */

#ifndef _DEVCONSOLE
#define _DEVCONSOLE

extern int	DevConsoleConvertKeystroke();
extern void	DevConsoleInputProc();
extern int	DevConsoleRawProc();

#endif /* _DEVCONSOLE */
