/* 
 * Rpc_GetName.c --
 *
 *	Rpc_GetName library routine.
 *
 * Copyright 1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that this copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/lib/c/etc/RCS/Rpc_GetName.c,v 1.2 91/04/12 18:46:50 kupfer Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <rpc.h>
#include <stdio.h>
#include <string.h>

static char *rpcNames[] = {
        "Bad command",
        "Echo 1",
        "Echo 2",
        "Send",
        "Receive",
        "Gettime",
        "Fs prefix",
        "Fs open",
        "Fs read",
        "Fs write",
        "Fs close",
        "Fs unlink",
        "Fs rename",
        "Fs mkdir",
        "Fs rmdir",
        "Fs mkdev",
        "Fs link",
        "Fs sym_link",
        "Fs get_attr",
        "Fs set_attr",
        "Fs get_attr_path",
        "Fs set_attr_path",
        "Fs get_io_attr",
        "Fs set_io_attr",
        "Fs dev_open",
        "Fs select",
        "Fs io_control",
        "Fs consist",
        "Fs consist_reply",
        "Fs copy_block",
        "Fs migrate",
        "Fs release",
        "Fs reopen",
        "Fs recovery",
        "Fs domain_info",
        "Proc mig_command",
        "Proc remote_call",
        "Proc remote_wait",
        "Proc getpcb",
        "Remote wakeup",
        "Sig send",
        "Fs release_new",
};

static int numNames = sizeof(rpcNames) / sizeof(char *);
  

/*
 *----------------------------------------------------------------------
 *
 * Rpc_GetName --
 *
 *	Return the human-readable name for an RPC.
 *
 * Results:
 *	Copies the name into namePtr, or as much as will fit.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Rpc_GetName(rpcNum, resultLen, resultPtr)
    int rpcNum;			/* which RPC */
    int resultLen;		/* how large is the array */
    char *resultPtr;		/* where to put the name */
{
    char tempName[RPC_MAX_NAME_LENGTH];
    char *whichName;

    if (rpcNum < 0 || rpcNum >= numNames) {
	sprintf(tempName, "Rpc <%d>", rpcNum);
	whichName = tempName;
    } else {
	whichName = rpcNames[rpcNum];
    }

    /*
     * The -1 is to allow room for the trailing null, in case 
     * "whichName" is too big for the buffer..
     */
    (void)strncpy(resultPtr, whichName, resultLen-1);
    resultPtr[resultLen-1] = '\0';
}
