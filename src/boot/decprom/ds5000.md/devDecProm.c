/* 
 * devDecProm.c --
 *
 *	Routines that access the Dec PROM device drivers.  This code is
 *	based on DecOS bootstrap code in boot/os/devio.c
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

#ifdef notdef
static char rcsid[] = "$Header: /sprite/src/boot/decprom/ds3100.md/RCS/devDecProm.c,v 1.1 90/02/16 16:14:07 shirriff Exp $ SPRITE (Berkeley)";
#endif /* not lint */


#include "sprite.h"
#include "user/fs.h"
#include "kernel/dev.h"
#include "kernel/devFsOpTable.h"
#include "boot.h"
#define _MONFUNCS
#include "kernel/machMon.h"

#define DEV_BSIZE 8192

#define READ 0
#define WRITE 1

#define NO_PRINTF

/*
 *----------------------------------------------------------------------
 *
 * DecPromDevOpen --
 *
 *	Open the device used for booting.  This depends on the initialization
 *	of the devicePtr->data field done in Dev_Config.
 *
 * Results:
 *	SUCCESS or FAILURE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
DecPromDevOpen(devicePtr)
    Fs_Device	*devicePtr;	/* Sprite device description */
{
    int fd;
    char 	*boot;

#ifndef NO_PRINTF
    Mach_MonPrintf("DecPromDevOpen\n");
#endif
    boot = Mach_MonGetenv("boot");
#ifndef NO_PRINTF
    Mach_MonPrintf("boot = %s\n", boot);
#endif
    fd = Mach_MonOpen(boot,READ);
    if (fd>0) {
	devicePtr->unit = fd;
	return SUCCESS;
    } else {
#ifndef NO_PRINTF
	Mach_MonPrintf("Open failure\n");
#endif
	return FAILURE;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * DecPromDevRead --
 *
 *	Read from the boot device used for booting.
 *
 * Results:
 *	SUCCESS or FAILURE.
 *
 * Side effects:
 *	The read operation.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
DecPromDevRead(devicePtr, offset, len, buffer, numBytesPtr)
    Fs_Device	*devicePtr;	/* Sprite device description */
    int offset;			/* Byte offset */
    int len;			/* Byte count;
    char *buffer;		/* Address to read into */
    int *numBytesPtr;		/* Return, the amount actually read */
{
    register int fd = devicePtr->unit;
    register int numBytes;
    register int totalBytes;
    register int toRead;
    int status;

    status = Mach_MonLseek(fd, offset, 0);
    if (status<0) {
#ifndef NO_PRINTF
	Mach_MonPrintf("Lseek failure\n");
#endif
	return FAILURE;
    }

    /*
     * Break the I/O in to chunks that are edible by the device.
     */
    totalBytes = 0;
    while (len > 0) {
	if (len > DEV_BSIZE) {
	    toRead = DEV_BSIZE;
	} else {
	    toRead = len;
	}
	numBytes = Mach_MonRead(fd, buffer, toRead);
	if (numBytes <= 0) {
	    break;
	}
	buffer += numBytes;
	len -= numBytes;
	totalBytes += numBytes;
    }
    *numBytesPtr = totalBytes;
    if (numBytes <= 0) {
#ifndef NO_PRINTF
	Mach_MonPrintf("Read failure: read(%d, %x, %x)\n",fd,buffer,toRead);
#endif
	return(FAILURE);
    } else {
	return(SUCCESS);
    }
}
