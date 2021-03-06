/* 
 * Fs_WriteVector.c --
 *
 *	Source code for the Fs_WriteVector library procedure.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: Fs_WriteVector.c,v 1.5 88/07/29 17:08:39 ouster Exp $ SPRITE (Berkeley)";
#endif not lint

#include <sprite.h>
#include <fs.h>
#include <status.h>
#include <stdlib.h>


/*
 *----------------------------------------------------------------------
 *
 * Fs_WriteVector --
 *
 *      The "normal" Fs_WriteVector routine for user code.  Write to the file
 *      indicated by the stream ID from the buffers described in vectorArray.
 *	The vectorArray indicates how much data to write, and amtWrittenPtr 
 *	is an output parameter that indicates how much data were written.  
 *
 *	Restarting from a signal is automatically handled by Fs_Write.
 *
 * Results:
 *	Result from Fs_Write.
 *
 * Side effects:
 *	See Fs_Write.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
Fs_WriteVector(streamID, numVectors, vectorArray, amtWrittenPtr)
    int         streamID;	/* The user's index into its open file list. */
    int         numVectors;	/* The # of vectors in userVectorArray. */
    Fs_IOVector vectorArray[];	/* The vectors defining where and how much to
				 * write. */
    int		*amtWrittenPtr;	/* The amount of bytes actually written. */
{
    register int 	i;
    register Fs_IOVector *vectorPtr;
    register int	bufSize;
    Address		buffer;
    Address		ptr;
    ReturnStatus	status;

    /*
     * Calculate the total number of bytes to be write.
     */
    bufSize = 0;
    for (i = 0, vectorPtr = vectorArray; i < numVectors; i++, vectorPtr++) {
	if (vectorPtr->bufSize < 0) {
	    return SYS_INVALID_ARG;
	}
	bufSize += vectorPtr->bufSize;
    }
    buffer = (Address) malloc((unsigned) bufSize);

    /*
     * Copy the data from the individual buffers specified in the 
     * vectorArray to the big buffer so the data can be written all at
     * once.
     */
    ptr = buffer;
    for (i = 0, vectorPtr = vectorArray; i < numVectors; i++, vectorPtr++) {
	bcopy(vectorPtr->buffer, ptr, vectorPtr->bufSize);
	ptr += vectorPtr->bufSize;
    }

    status = Fs_Write(streamID, bufSize, buffer, amtWrittenPtr);
    free((char *) buffer);
    return(status);
}
