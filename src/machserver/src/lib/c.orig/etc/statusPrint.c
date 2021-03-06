/*
 * statusPrint.c --
 *
 * 	Prints the message associated with a status value in 
 *	the status.h file.
 *
 * Copyright 1986 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/lib/c/etc/RCS/statusPrint.c,v 1.1 89/10/24 12:27:04 jhh Exp $ SPRITE (Berkeley)";
#endif not lint

#include <sprite.h>
#include <status.h>
#include <stdio.h>

/*
 *----------------------------------------------------------------------
 *
 * Stat_PrintMsg --
 *
 *	Output an error message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A message gets printed on standard error, of the form
 *	"string: message", where "string" is the argument to this
 *	procedure and "message" is the standard error message
 *	associated with "status".
 *
 *----------------------------------------------------------------------
 */

void
Stat_PrintMsg(status, string)
    ReturnStatus status;		/* Error status. */
    char *string;			/* Identifying string to output before
					 * the error message string. */
{
    if (string == NULL) {
	fprintf(stderr, "%s\n", Stat_GetMsg(status));
    } else {
	fprintf(stderr, "%s: %s\n", string, Stat_GetMsg(status));
    }
}

