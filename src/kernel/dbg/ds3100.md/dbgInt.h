/*
 * dbgInt.h --
 *
 *     	Internal types, constants,  and procedure headers for the debugger
 *	module.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * $Header: /cdrom/src/kernel/Cvsroot/kernel/dbg/ds3100.md/dbgInt.h,v 9.1 90/09/24 17:14:18 kupfer Exp $ SPRITE (Berkeley)
 */

#ifndef _DBGINT
#define _DBGINT

extern	int	dbgTraceLevel; 		/* Our trace level. */

extern unsigned int *
		DbgGetDestPC _ARGS_((Address instPC));

#endif /* _DBGINT */
