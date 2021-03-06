/* 
 * fileLoad.c --
 *
 *	The routine to load a program into main memory.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifdef notdef
static char rcsid[] = "$Header: /sprite/src/boot/decprom/RCS/fileLoad.c,v 1.1 90/02/16 16:14:08 shirriff Exp $ SPRITE (Berkeley)";
#endif not lint

#include "sprite.h"
#include "fsBoot.h"
#include "kernel/procMach.h"
#include "kernel/machMon.h"
#include "boot.h"

#define KERNEL_ENTRY KERNEL_START


/*
 *----------------------------------------------------------------------
 *
 * FileLoad --
 *
 *	Read in the kernel object file.  This is loaded into memory at
 *	a pre-defined location (in spite of what is in the a.out header)
 *	for compatibility with Sun/UNIX boot programs.  The Sprite kernel
 *	expects to be loaded into the wrong place and does some re-mapping
 *	to relocate the kernel into high virtual memory.
 *
 * Results:
 *	The entry point.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */


int
FileLoad(handlePtr)
    register Fsio_FileIOHandle	*handlePtr;
{
    ProcExecHeader	aout;
    int			bytesRead;
    register int	*addr;
    register ReturnStatus status;
    register int	i;
    register int	numBytes;

    /*
     * Read a.out header.
     */

    status = Fs_Read(handlePtr, 0, sizeof(aout), &aout, &bytesRead);
    if (status != SUCCESS || bytesRead != sizeof(aout)) {
	Mach_MonPrintf("No a.out header");
	goto readError;
    } else if (aout.aoutHeader.magic != PROC_OMAGIC) {
	Mach_MonPrintf("A.out? mag %x size %d+%d+%d\n",
	    aout.aoutHeader.magic, aout.aoutHeader.codeSize,
	    aout.aoutHeader.heapSize, aout.aoutHeader.bssSize);
	return(-1);
    }

    /*
     * Read the code and initialized data.
     */

    numBytes = aout.aoutHeader.codeSize + aout.aoutHeader.heapSize;
    Mach_MonPrintf("Size: %d+%d", aout.aoutHeader.codeSize,
	    aout.aoutHeader.heapSize);
    status = Fs_Read(handlePtr, PROC_CODE_FILE_OFFSET(aout), numBytes,
		      aout.aoutHeader.codeStart, &bytesRead);

    if (status != SUCCESS) {
readError:
	Mach_MonPrintf("\nRead error <%x>\n", status);
	return(-1);
    } else if (bytesRead != numBytes) {
shortRead:
	Mach_MonPrintf("\nShort read (%d)\n", bytesRead);
	return(-1);
    }

    /*
     * Zero out the bss.
     */

    numBytes = aout.aoutHeader.bssSize;
    Mach_MonPrintf("+%d\n", numBytes);
    addr = (int *) (KERNEL_ENTRY + aout.aoutHeader.codeSize +
	    aout.aoutHeader.bssSize);
    bzero(addr, numBytes);

    return ((int)aout.aoutHeader.entry);
}
