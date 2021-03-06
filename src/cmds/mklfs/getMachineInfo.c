/* 
 * getMachineInfo.c --
 *
 *	Routine to get infomation needed for LFS file systems from a
 *	machine.
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
static char rcsid[] = "$Header: /sprite/src/admin/mklfs/RCS/getMachineInfo.c,v 1.1 91/05/31 11:09:31 mendel Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#ifdef __STDC__
/*
 * If we are compiling on a machine that has a ASCII C compiler, set the
 * define _HAS_PROTOTYPES which causes the Sprite header files to 
 * expand function definitions to included prototypes.
 */
#define	_HAS_PROTOTYPES 
#endif /* __STDC__ */

#include <sprite.h>
#include <varargs.h>
#include <stdio.h>
#include <cfuncproto.h>
#include <stdlib.h>
#include "getMachineInfo.h"

#include <kernel/fs.h>
#include <kernel/fsStat.h>
#include <fsCmd.h>


/*
 *----------------------------------------------------------------------
 *
 * GetMachineInfo --
 *
 *	Routine to get infomation needed for LFS file systems from a
 *	machine.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GetMachineInfo(serverIDPtr, maxNumCacheBlocksPtr)
    int	*serverIDPtr;	/* OUT: Server ID of machine. */
    int	*maxNumCacheBlocksPtr; /* OUT: Maximum number of cache blocks. */
{
    Fs_Stats fsStats;
    ReturnStatus status;
    extern ReturnStatus Proc_GetHostIDs 
			_ARGS_((int *virtualHostPtr, int *physicalHostPtr));
    (void)Proc_GetHostIDs(serverIDPtr, (int *) NULL);
    status = Fs_Command(FS_RETURN_STATS, sizeof(Fs_Stats), (char *)&fsStats);
    if (status != SUCCESS) {
	Stat_PrintMsg(status, "Fs_Command failed");
	exit(status);
    }
    *maxNumCacheBlocksPtr = fsStats.blockCache.maxNumBlocks;
}

