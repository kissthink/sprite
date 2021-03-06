/*
 * prefixInt.h --
 *
 *	Declarations of data structures and variables private to the 
 *	fsprefix module.
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
 * $Header: /r3/kupfer/spriteserver/src/sprited/fsprefix/RCS/fsprefixInt.h,v 1.2 91/12/01 21:58:57 kupfer Exp $ SPRITE (Berkeley)
 */

#ifndef _FSPREFIXINT
#define _FSPREFIXINT

#include <fsprefix.h>

/* constants */

/* data structures */

/*
 * A list of hosts that can use a domain is kept for exported prefixes.
 */
typedef struct FsprefixExport {
    List_Links	links;
    int		spriteID;
} FsprefixExport;

/* procedures */

extern void FsprefixDone _ARGS_((Fsprefix *prefixPtr));
extern void FsprefixHandleCloseInt _ARGS_((Fsprefix *prefixPtr, int flags));
extern void FsprefixIterate _ARGS_((Fsprefix **prefixPtrPtr));

#endif /* _FSPREFIXINT */

