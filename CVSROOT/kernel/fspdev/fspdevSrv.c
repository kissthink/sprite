/* 
 * fsPdev.c --  
 *
 *	This file contains routines directly related to the request-response
 *	protocol between the client and server processes.  Routines that
 *	setup state, i.e. the SrvOpen, CltOpen, and migration routines,
 *	are in fsPdevSetup.c.  Routines for the Control stream are
 *	found in fsPdevControl.c
 *
 *	Operations are forwarded to a user-level server process using
 *	a "request-response" protocol.
 *	The server process declares a request buffer and optionally a read
 *	ahead buffer in its address space.  The kernel puts requests, which
 *	are generated when a client does an operation on the pseudo stream,
 *	into the request buffer directly.  The server learns of new requests
 *	by reading messages from the server stream that contain offsets within
 *	the request buffer.  Write requests may not require a response from
 *	the server.  Instead the kernel just puts the request into the buffer
 *	and returns to the client.  This allows many requests to be buffered
 *	before a context switch to the server process.  Similarly,
 *	the server can put read data into the read ahead buffer for a pseudo
 *	stream.  In this case a client's read will be satisfied from the
 *	buffer and the server won't be contacted.
 *
 * Copyright 1987, 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header$ SPRITE (Berkeley)";
#endif not lint

#include "sprite.h"
#include "fs.h"
#include "fsInt.h"
#include "fsOpTable.h"
#include "fsFile.h"
#include "fsStream.h"
#include "fsClient.h"
#include "fsMigrate.h"
#include "fsLock.h"
#include "fsDisk.h"
#include "proc.h"
#include "rpc.h"
#include "swapBuffer.h"

/*
 * Prevent tracing by defining CLEAN here before this next include
 */
#undef CLEAN
#include "fsPdev.h"
#include "dev/pfs.h"

/*
 * Access to PdevServerIOHandle is monitored.
 */
#define LOCKPTR (&pdevHandlePtr->lock)

/*
 * Flags for a pseudo device handle.
 *	PDEV_SETUP		The server has set up state for the stream.
 *	PDEV_BUSY		Set during request-response to ensure that
 *				only one client process is using the stream.
 *	PDEV_REPLY_READY	The server gave us a reply
 *	PDEV_REPLY_FAILED	There was some problem (usually with copy in)
 *				getting the reply from the server.
 *	PDEV_SERVER_GONE	Set after the server closes its stream.
 *	PDEV_READ_BUF_EMPTY	When set there is no data in the read ahead
 *				buf.
 *	PDEV_SERVER_KNOWS_IT	Set after the server has done a read and been
 *				told that we too think the read ahead buffer
 *				is empty.  This synchronization simplifies
 *				things; there is no wrapping; there is no
 *				confusion about full vs. empty.
 *	PDEV_READ_PTRS_CHANGED	Set when we have used data from the read
 *				ahead buffer.  This makes the server stream
 *				selectable so it will find out about the
 *				new pointers.
 *	PDEV_WRITE_BEHIND	Write-behind is enabled for this stream.
 *	PDEV_NO_BIG_WRITES	Causes writes larger than will fit into
 *				the request buffer to fail.  This is to
 *				support UDP socket semantics, sigh.
 *	FS_USER			This flag is borrowed from the stream flags
 *				and it indicates the buffers are in user space
 */
#define PDEV_SETUP		0x0001
#define PDEV_BUSY		0x0002
#define PDEV_REPLY_READY	0x0004
#define PDEV_REPLY_FAILED	0x0008
#define PDEV_SERVER_GONE	0x0010
#define PDEV_READ_BUF_EMPTY	0x0020
#define PDEV_SERVER_KNOWS_IT	0x0040
#define PDEV_READ_PTRS_CHANGED	0x0080
#define PDEV_WRITE_BEHIND	0x0100
#define PDEV_NO_BIG_WRITES	0x0200
/*resrv FS_USER			0x8000 */

/*
 * Forward declarations.
 */

static	ReturnStatus		RequestResponse();
static	void			PdevClientWakeup();
static	void			PdevClientNotify();
void				FsRmtPseudoStreamClose();

/*
 *----------------------------------------------------------------------
 *
 * RequestResponse --
 *
 *	The general request-response protocol between a client's pseudo
 *	stream and the server stream.  This passes a request to the server
 *	that specifies the command and parameters, the size of the input data
 *	and the max size of the reply.  Then, if a reply is needed, this blocks
 *	the client until the server responds.
 *
 * Results:
 *	An error code from the server, or from errors on the vm copies.
 *
 * Side effects:
 *	The server process is locked while we copy the request into the
 *	buffer in the server's address space.  The firstByte and lastByte
 *	offsets into the pseudo stream request buffer are updated to
 *	reflect the new request.  The corresponding server stream is
 *	marked FS_READABLE.
 *
 *----------------------------------------------------------------------
 */

INTERNAL static ReturnStatus
RequestResponse(pdevHandlePtr, hdrSize, requestHdrPtr, inputSize, inputBuf,
	replySize, replyBuf, replySizePtr, waitPtr)
    register PdevServerIOHandle *pdevHandlePtr;	/* Caller should lock this
						 * with the monitor lock. */
    int			hdrSize;	/* Either sizeof(Pdev_Request) or
					 * sizeof(Pfs_Request) */
    register Pdev_RequestHdr *requestHdrPtr;	/* Caller fills in the
					 * command and parameter parts. The
					 * sizes are filled in here. */
    int			inputSize;	/* Size of input buffer. */
    Address		inputBuf;	/* Inputs of the remote command. */
    int			replySize;	/* Size of output buffer.  0 means
					 * no reply data expected.  Of the
					 * operation is PDEV_WRITE_ASYNC then
					 * no reply is wanted and ths argument
					 * is ignored. */
    Address		replyBuf;	/* Results of the remote command. */
    int			*replySizePtr;	/* Amount of data actually in replyBuf.
					 * (May be NIL if not needed.) */
    Sync_RemoteWaiter	*waitPtr;	/* Client process info for waiting.
					 * Only needed for read & write. */
{
    register ReturnStatus  status;
    Proc_ControlBlock	   *serverProcPtr;   /* For VM copy operations */
    register int	   firstByte;	     /* Offset into request buffer */
    register int	   lastByte;	     /* Offset into request buffer */
    int			   room;	     /* Room available in req. buf.*/
    int			   savedLastByte;    /* For error recovery */
    int			   savedFirstByte;   /*   ditto */

    if (replySizePtr != (int *) NIL) {
	*replySizePtr = 0;
    }

    if ((pdevHandlePtr == (PdevServerIOHandle *)NIL) ||
	(pdevHandlePtr->flags & PDEV_SERVER_GONE)) {
	return(DEV_OFFLINE);
    } else if ((pdevHandlePtr->flags & PDEV_SETUP) == 0) {
	panic( "RequestResponse: connection not set up\n");
    }
    /*
     * See if we have to switch to a new request buffer.  This is needed
     * to support UDP, which wants to set a maximum write size.  The max
     * is implemented by letting the UDP server change the buffer size and
     * setting the property that writes larger than the buffer fail.  We
     * wait and switch after the request buffer empties.
     */
    firstByte = pdevHandlePtr->requestBuf.firstByte;
    lastByte = pdevHandlePtr->requestBuf.lastByte;
    if ((pdevHandlePtr->nextRequestBuffer != (Address)NIL) &&
	((firstByte > lastByte) || (firstByte == -1))) {
	printf( "Switching to request buffer at 0x%x\n",
			       pdevHandlePtr->nextRequestBuffer);
	pdevHandlePtr->requestBuf.data = pdevHandlePtr->nextRequestBuffer;
	pdevHandlePtr->requestBuf.size = pdevHandlePtr->nextRequestBufSize;
	pdevHandlePtr->nextRequestBuffer = (Address)NIL;
	firstByte = -1;
    }
    /*
     * FORMAT THE REQUEST HEADER.  Note that the complete message size is
     * rounded up so subsequent messages start on word boundaries.
     */
    if (hdrSize == sizeof(Pdev_Request)) {
	requestHdrPtr->magic = PDEV_REQUEST_MAGIC;
    } else if (hdrSize == sizeof(Pfs_Request)) {
	requestHdrPtr->magic = PFS_REQUEST_MAGIC;
    } else {
	panic( "RequestResponse: bad hdr size\n");
    }
    requestHdrPtr->requestSize = inputSize;
    requestHdrPtr->replySize = replySize;
    requestHdrPtr->messageSize = hdrSize +
		((inputSize + sizeof(int) - 1) / sizeof(int)) * sizeof(int);
    if (pdevHandlePtr->requestBuf.size < requestHdrPtr->messageSize) {
	printf( "RequestResponse request too large\n");
	return(GEN_INVALID_ARG);
    }

    PDEV_REQUEST_PRINT(&pdevHandlePtr->hdr.fileID, requestHdrPtr);
    PDEV_REQUEST(&pdevHandlePtr->hdr.fileID, requestHdrPtr);

    /*
     * PUT THE REQUEST INTO THE REQUEST BUFFER.
     * We assume that our caller will not give us a request that can't all
     * fit into the buffer.  However, if the buffer is not empty enough we
     * wait for the server to catch up with us.  (Things could be optimized
     * perhaps to wait for just enough room.  To be real clever you have
     * to be careful when you reset the firstByte so that the server only
     * notices after it has caught up with everything at the end of the
     * buffer.  To keep things simple we just wait for the server to catch up
     * completely if we can't fit this request in.)
     */

    savedFirstByte = firstByte;
    savedLastByte = lastByte;
    if (firstByte > lastByte || firstByte == -1) {
	/*
	 * Buffer has emptied.
	 */
	firstByte = 0;
	pdevHandlePtr->requestBuf.firstByte = firstByte;
	lastByte = requestHdrPtr->messageSize - 1;
    } else {
	room = pdevHandlePtr->requestBuf.size - (lastByte + 1);
	if (room < requestHdrPtr->messageSize) {
	    /*
	     * There is no room left at the end of the buffer.
	     * We wait and then put the request at the beginning.
	     */
	    while (pdevHandlePtr->requestBuf.firstByte <
		   pdevHandlePtr->requestBuf.lastByte) {
		DBG_PRINT( (" (catch up) ") );
		(void) Sync_Wait(&pdevHandlePtr->caughtUp, FALSE);
		if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
		    status = DEV_OFFLINE;
		    goto failure;
		}
	    }
	    savedFirstByte = -1;
	    firstByte = 0;
	    pdevHandlePtr->requestBuf.firstByte = firstByte;
	    lastByte = requestHdrPtr->messageSize - 1;
	} else {
	    /*
	     * Append the message to the other requests.
	     */
	    firstByte = lastByte + 1;
	    lastByte += requestHdrPtr->messageSize;
	}
    }
    pdevHandlePtr->requestBuf.lastByte = lastByte;
    DBG_PRINT( (" first %d last %d\n", firstByte, lastByte) );
    pdevHandlePtr->operation = requestHdrPtr->operation;
    if (pdevHandlePtr->operation == PFS_OPEN) {
	/*
	 * We have to snarf up the hostID of the client doing the open
	 * so the new pseudo-device connection that may be set up as
	 * a side effect of PFS_OPEN is set up right.  The useFlags are
	 * needed to initialize the new client stream right.
	 */
	register Pfs_Request *pfsRequestPtr = (Pfs_Request *)requestHdrPtr;
	pdevHandlePtr->open.clientID = pfsRequestPtr->param.open.clientID;
	pdevHandlePtr->open.useFlags = pfsRequestPtr->param.open.useFlags;
	pdevHandlePtr->open.name = (char *)inputBuf;
    }

    /*
     * COPY REQUEST AND DATA.
     * Copy the request and data out into the server's request buffer.
     * We map to a proc table pointer for the server which has a side
     * effect of locking down the server process so it can't disappear.
     */

    serverProcPtr = Proc_LockPID(pdevHandlePtr->serverPID);
    if (serverProcPtr == (Proc_ControlBlock *)NIL) {
	status = DEV_OFFLINE;
	goto failure;
    }
    status = Vm_CopyOutProc(hdrSize, (Address)requestHdrPtr, TRUE,
			    serverProcPtr, (Address)
			    &pdevHandlePtr->requestBuf.data[firstByte]);
    if (status == SUCCESS) {
	firstByte += hdrSize;
	if (inputSize > 0) {
	    status = Vm_CopyOutProc(inputSize, inputBuf, 
			(pdevHandlePtr->flags & FS_USER) == 0, serverProcPtr,
			(Address)&pdevHandlePtr->requestBuf.data[firstByte]);
	}
    }
    Proc_Unlock(serverProcPtr);
    if (status != SUCCESS) {
	/*
	 * Either the message header or data couldn't get copied out.
	 * Reset the buffer pointers so the bad request isn't seen.
	 */
	pdevHandlePtr->requestBuf.firstByte = savedFirstByte;
	pdevHandlePtr->requestBuf.lastByte = savedLastByte;
	goto failure;
    }

    /*
     * POKE THE SERVER so it can read the new pointer values.
     * This is done here even if write-behind is enabled, even though our
     * scheduler tends to wake up the server too soon.
     * Although it is possible to put a notify in about 3 other places
     * to catch cases where the client does a write-behind and then waits,
     * not all clients are clever enough to use select.  That solution results
     * in cases where a write can linger a long time in the request buffer.
     * (cat /dev/syslog in a tx window is a good test case.)
     */
    FsFastWaitListNotify(&pdevHandlePtr->srvReadWaitList);

    if (pdevHandlePtr->operation != PDEV_WRITE_ASYNC) {
	/*
	 * WAIT FOR A REPLY.
	 * We save the client's reply buffer address and processID in the
	 * stream state so the kernel can copy the reply directly from
	 * the server's address space to the client's when the server
	 * makes the IOC_PDEV_REPLY IOControl.
	 */

	pdevHandlePtr->replyBuf = replyBuf;
	pdevHandlePtr->clientPID = (Proc_GetEffectiveProc())->processID;
	if (waitPtr != (Sync_RemoteWaiter *)NIL) {
	    pdevHandlePtr->clientWait = *waitPtr;
	}
	pdevHandlePtr->flags &= ~PDEV_REPLY_READY;
	while ((pdevHandlePtr->flags & PDEV_REPLY_READY) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->replyReady, FALSE);
	    if (pdevHandlePtr->flags & (PDEV_REPLY_FAILED|PDEV_SERVER_GONE)) {
		status = DEV_OFFLINE;
		goto failure;
	    }
	}
	if (replySizePtr != (int *) NIL) {
	    *replySizePtr = pdevHandlePtr->reply.replySize;
	}
    } else {
	pdevHandlePtr->reply.status = SUCCESS;
    }
failure:

    if (status == SUCCESS) {
	status = pdevHandlePtr->reply.status;
    }
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsServerStreamCreate --
 *
 *	Set up the stream state for a server's private channel to a client.
 *	This creates a PdevServerIOHandle that has all the state for the
 *	connection to the client.
 *
 * Results:
 *	A pointer to the I/O handle created.  The handle is locked.
 *	NIL is returned if a handle under the fileID already existed.
 *
 * Side effects:
 *	The I/O handle for this connection between a client and the server
 *	is installed and initialized.
 *
 *----------------------------------------------------------------------
 */

PdevServerIOHandle *
FsServerStreamCreate(ioFileIDPtr, name)
    Fs_FileID	*ioFileIDPtr;	/* File ID used for pseudo stream handle */
    char	*name;		/* File name for error messages */
{
    FsHandleHeader *hdrPtr;
    register PdevServerIOHandle *pdevHandlePtr;
    Boolean found;

    ioFileIDPtr->type = FS_SERVER_STREAM;
    found = FsHandleInstall(ioFileIDPtr, sizeof(PdevServerIOHandle), name,
			    &hdrPtr);
    pdevHandlePtr = (PdevServerIOHandle *)hdrPtr;
    if (found) {
	printf( "ServerStreamCreate, found handle <%x,%x,%x>\n",
		  hdrPtr->fileID.serverID, hdrPtr->fileID.major,
		  hdrPtr->fileID.minor);
	FsHandleRelease(pdevHandlePtr, TRUE);
	return((PdevServerIOHandle *)NIL);
    }

    DBG_PRINT( ("ServerStreamOpen <%d,%x,%x>\n",
	    ioFileIDPtr->serverID, ioFileIDPtr->major, ioFileIDPtr->minor) );

    /*
     * Initialize the state for the pseudo stream.  Remember that
     * the request and read ahead buffers for the pseudo-stream are set up
     * via IOControls by the server process later.  In the meantime we
     * pretend the connection is busy to lock out requests.
     */

    pdevHandlePtr->flags = PDEV_BUSY;
    pdevHandlePtr->selectBits = 0;

    pdevHandlePtr->requestBuf.data = (Address)NIL;
    pdevHandlePtr->requestBuf.firstByte = -1;
    pdevHandlePtr->requestBuf.lastByte = -1;
    pdevHandlePtr->requestBuf.size = 0;

    pdevHandlePtr->readBuf.data = (Address)NIL;
    pdevHandlePtr->readBuf.firstByte = -1;
    pdevHandlePtr->readBuf.lastByte = -1;
    pdevHandlePtr->readBuf.size = 0;

    pdevHandlePtr->nextRequestBuffer = (Address)NIL;

    pdevHandlePtr->operation = 0;
    pdevHandlePtr->replyBuf = (Address)NIL;
    pdevHandlePtr->serverPID = (Proc_PID)NIL;
    pdevHandlePtr->clientPID = (Proc_PID)NIL;
    pdevHandlePtr->clientWait.pid = NIL;
    pdevHandlePtr->clientWait.hostID = NIL;
    pdevHandlePtr->clientWait.waitToken = NIL;

    List_Init(&pdevHandlePtr->srvReadWaitList);
    List_Init(&pdevHandlePtr->cltReadWaitList);
    List_Init(&pdevHandlePtr->cltWriteWaitList);
    List_Init(&pdevHandlePtr->cltExceptWaitList);

    pdevHandlePtr->ctrlHandlePtr = (PdevControlIOHandle *)NIL;
    pdevHandlePtr->userLevelID = *ioFileIDPtr;

    return(pdevHandlePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FsServerStreamSelect --
 *
 *	Select a server's request/response stream.  This returns
 *	FS_READABLE in the outFlags if there is data in the
 *	server's request buffer.  The next read on the server stream
 *	will return the current pointers into the buffer.
 *
 * Results:
 *	SUCCESS
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
FsServerStreamSelect(hdrPtr, waitPtr, readPtr, writePtr, exceptPtr)
    FsHandleHeader	*hdrPtr;	/* Handle on device to select */
    Sync_RemoteWaiter	*waitPtr;	/* Process info for remote waiting */
    int 		*readPtr;	/* Bit to clear if non-readable */
    int 		*writePtr;	/* Bit to clear if non-writeable */
    int 		*exceptPtr;	/* Bit to clear if non-exceptable */
{
    register PdevServerIOHandle	*pdevHandlePtr = (PdevServerIOHandle *)hdrPtr;

    LOCK_MONITOR;
    if (*readPtr) {
	if (((pdevHandlePtr->flags & PDEV_READ_PTRS_CHANGED) == 0) &&
	     ((pdevHandlePtr->requestBuf.firstByte == -1) ||
	      (pdevHandlePtr->requestBuf.firstByte >=
		  pdevHandlePtr->requestBuf.lastByte))) {
	    *readPtr = 0;
	    FsFastWaitListInsert(&pdevHandlePtr->srvReadWaitList, waitPtr);
	}
    }
    *writePtr = 0;
    *exceptPtr = 0;
    UNLOCK_MONITOR;
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * FsServerStreamRead --
 *
 *	When the server reads on a server stream it is looking for a
 *	message containing pointers into the request buffer that's
 *	in its address spapce.  This routine returns those values.
 *
 * Results:
 *	SUCCESS unless all clients have gone away.
 *
 * Side effects:
 *	The buffer is filled a Pdev_BufPtrs structure.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
FsServerStreamRead(streamPtr, flags, buffer, offsetPtr, lenPtr, waitPtr)
    register Fs_Stream 	*streamPtr;	/* Stream to read from. */
    int		flags;		/* Flags from stream */
    Address 	buffer;		/* Where to read into. */
    int		*offsetPtr;	/* In/Out byte offset for the read */
    int 	*lenPtr;	/* In/Out byte count parameter */
    Sync_RemoteWaiter *waitPtr;	/* Process info for waiting */
{
    register PdevServerIOHandle *pdevHandlePtr =
	    (PdevServerIOHandle *)streamPtr->ioHandlePtr;
    register ReturnStatus status;
    Pdev_BufPtrs bufPtrs;
    register int reqFirstByte, reqLastByte;

    LOCK_MONITOR;
    /*
     * The server stream is readable only if there are requests in the
     * request buffer or if the read ahead buffers have changed since
     * the last time the server did a read.
     */
    reqFirstByte = pdevHandlePtr->requestBuf.firstByte;
    reqLastByte = pdevHandlePtr->requestBuf.lastByte;
    if (reqFirstByte > pdevHandlePtr->requestBuf.size ||
	reqLastByte > pdevHandlePtr->requestBuf.size) {
	panic( "PdevServerRead, pointers inconsistent\n");
	UNLOCK_MONITOR;
	return(GEN_INVALID_ARG);
    }
    if (((pdevHandlePtr->flags & PDEV_READ_PTRS_CHANGED) == 0) &&
	((reqFirstByte == -1) || (reqFirstByte > reqLastByte))) {
	status = FS_WOULD_BLOCK;
	*lenPtr = 0;
	FsFastWaitListInsert(&pdevHandlePtr->srvReadWaitList, waitPtr);
	PDEV_TRACE(&pdevHandlePtr->hdr.fileID, PDEVT_SRV_READ_WAIT);
    } else {
	/*
	 * Copy the current pointers out to the server.  We include the
	 * server's address of the request buffer to support changing
	 * the request buffer after requests have started to flow.
	 */
	PDEV_TRACE(&pdevHandlePtr->hdr.fileID, PDEVT_SRV_READ);
	bufPtrs.magic = PDEV_BUF_PTR_MAGIC;
	bufPtrs.requestAddr = pdevHandlePtr->requestBuf.data;
	if ((reqFirstByte == -1) || (reqFirstByte > reqLastByte)) {
	    /*
	     * Request buffer is empty.
	     */
	    bufPtrs.requestFirstByte = -1;
	    bufPtrs.requestLastByte = -1;
	} else {
	    bufPtrs.requestFirstByte = reqFirstByte;
	    bufPtrs.requestLastByte = reqLastByte;
	}

	/*
	 * The read ahead buffer is filled by the server until there is
	 * no room left.  Only after the kernel has emptied the
	 * read ahead buffer will the server start filling it again.
	 * We use the PDEV_SERVER_KNOWS_IT state bit to know when to
	 * expect new pointer values after the buffer empties.
	 */
	if (pdevHandlePtr->flags & PDEV_READ_BUF_EMPTY) {
	    bufPtrs.readLastByte = -1;
	    bufPtrs.readFirstByte = -1;
	    pdevHandlePtr->flags |= PDEV_SERVER_KNOWS_IT;
	} else {
	    bufPtrs.readFirstByte = pdevHandlePtr->readBuf.firstByte;
	    bufPtrs.readLastByte = pdevHandlePtr->readBuf.lastByte;
	}
	pdevHandlePtr->flags &= ~PDEV_READ_PTRS_CHANGED;
	status = Vm_CopyOut(sizeof(Pdev_BufPtrs), (Address)&bufPtrs, buffer);
	*lenPtr = sizeof(Pdev_BufPtrs);
	/*
	 * Poke the "caughtUp" condition in case anyone is waiting to stuff
	 * more requests into the buffer.  (THIS SEEMS INAPPROPRIATE)
	 */
	Sync_Broadcast(&pdevHandlePtr->caughtUp);
	DBG_PRINT( ("READ %x,%x req %d:%d read %d:%d\n",
		pdevHandlePtr->hdr.fileID.major,
		pdevHandlePtr->hdr.fileID.minor,
		pdevHandlePtr->requestBuf.firstByte,
		pdevHandlePtr->requestBuf.lastByte,
		pdevHandlePtr->readBuf.firstByte,
		pdevHandlePtr->readBuf.lastByte) );
    }
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsServerStreamIOControl --
 *
 *	IOControls for the server's stream.  The server process uses this
 *	to manipulate the request and read ahead buffers in its address
 *	space.  It delcares them, and queries and sets pointers into them.
 *	The server also replies to requests here, and notifies the kernel
 *	when the pseudo-device is selectable.
 *
 * Results:
 *	SUCCESS if all went well, otherwise a status from a Vm operation
 *	or a consistency check.
 *
 * Side effects:
 *	This is the main entry point for the server process to control
 *	the pseudo-device connection to its clients.  The side effects
 *	depend on the I/O control.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ENTRY ReturnStatus
FsServerStreamIOControl(streamPtr, command, byteOrder, inBufPtr, outBufPtr)
    Fs_Stream	*streamPtr;	/* Stream to server handle. */
    int		command;	/* The control operation to be performed. */
    int		byteOrder;	/* Client's byte order, should be same */
    Fs_Buffer *inBufPtr;	/* Command inputs */
    Fs_Buffer *outBufPtr;	/* Buffer for return parameters */
{
    ReturnStatus	status = SUCCESS;
    register PdevServerIOHandle	*pdevHandlePtr =
	    (PdevServerIOHandle *)streamPtr->ioHandlePtr;

    LOCK_MONITOR;

    if (byteOrder != mach_ByteOrder) {
	panic( "FsServerStreamIOControl: wrong byte order\n");
    }
    switch (command) {
	case IOC_PDEV_SET_BUF: {
	    /*
	     * The server is declaring the buffer space used for requests
	     * and (optionally) for read ahead.
	     * To support UDP socket semantics, we let the server change
	     * the request buffer after things have already started up.
	     * (This is sort of a pain, as we have to let the current
	     * request buffer empty before switching.)
	     *
	     * Side effects:
	     *		Set the request, and optionally the read-ahead,
	     *		buffer pointers.  The setup condition is notified
	     *		to let the client's open transaction begin.
	     */
	    register Pdev_SetBufArgs *argPtr =
		    (Pdev_SetBufArgs *)inBufPtr->addr;
	    register Proc_ControlBlock *procPtr;
	    if (inBufPtr->size != sizeof(Pdev_SetBufArgs)) {
		status = GEN_INVALID_ARG;
	    } else if ((pdevHandlePtr->flags & PDEV_SETUP) == 0) {
		/*
		 * Normal case, first time initialization.  The state has
		 * been marked BUSY to simplify waiting in the routines
		 * that call RequestResponse, so we clear that state bit.
		 * We also mark it explicitly as SETUP so we can bail out
		 * later if the server never sets up the request buffer.
		 */
		pdevHandlePtr->requestBuf.data = argPtr->requestBufAddr;
		pdevHandlePtr->requestBuf.size = argPtr->requestBufSize;
		pdevHandlePtr->requestBuf.firstByte = -1;
		pdevHandlePtr->requestBuf.lastByte = -1;

		pdevHandlePtr->flags &= ~PDEV_BUSY;
		pdevHandlePtr->flags |= PDEV_SETUP|PDEV_READ_BUF_EMPTY|
						 PDEV_SERVER_KNOWS_IT;
		if (argPtr->readBufAddr == (Address)NIL ||
		    argPtr->readBufAddr == (Address)0) {
		    pdevHandlePtr->readBuf.data = (Address)NIL;
		} else {
		    pdevHandlePtr->readBuf.data = argPtr->readBufAddr;
		}
		pdevHandlePtr->readBuf.size = argPtr->readBufSize;
		pdevHandlePtr->readBuf.firstByte = -1;
		pdevHandlePtr->readBuf.lastByte = -1;

		procPtr = Proc_GetEffectiveProc();
		pdevHandlePtr->serverPID = procPtr->processID;

		Sync_Broadcast(&pdevHandlePtr->access);
	    } else {
		/*
		 * The server is changing request buffers.  We just remember
		 * the new buffer address and size here, and switch over
		 * in RequestResponse after the current request buffer empties.
		 */
		pdevHandlePtr->nextRequestBuffer = argPtr->requestBufAddr;
		pdevHandlePtr->nextRequestBufSize = argPtr->requestBufSize;
		if (pdevHandlePtr->nextRequestBuffer == (Address)0) {
		    pdevHandlePtr->nextRequestBuffer = (Address)NIL;
		}
	    }
	    break;
	}
	case IOC_PDEV_WRITE_BEHIND: {
	    /*
	     * Side effects:
	     *		Set/unset write-behind buffering in the request buffer.
	     */
	    register Boolean writeBehind;
	    if (inBufPtr->size < sizeof(Boolean)) {
		status = GEN_INVALID_ARG;
	    } else {
		writeBehind = *(Boolean *)inBufPtr->addr;
		if (writeBehind) {
		    pdevHandlePtr->flags |= PDEV_WRITE_BEHIND;
		} else {
		    pdevHandlePtr->flags &= ~PDEV_WRITE_BEHIND;
		}
	    }
	    break;
	}
	case IOC_PDEV_BIG_WRITES: {
	    /*
	     * Side effects:
	     *		Set/unset the client's ability to make large writes.
	     */
	    register Boolean allowLargeWrites;
	    if (inBufPtr->size < sizeof(Boolean)) {
		status = GEN_INVALID_ARG;
	    } else {
		allowLargeWrites = *(Boolean *)inBufPtr->addr;
		if (allowLargeWrites) {
		    pdevHandlePtr->flags &= ~PDEV_NO_BIG_WRITES;
		} else {
		    pdevHandlePtr->flags |= PDEV_NO_BIG_WRITES;
		}
	    }
	    break;
	}
	case IOC_PDEV_SET_PTRS: {
	    /*
	     * The server is telling us about new pointer values.  We only
	     * pay attention to the requestFirstByte and readLastByte as
	     * it is our job to modify the other pointers.
	     *
	     * Side effects:
	     *		Set requestBuf.firstByte and readBuf.lastByte if
	     *		the server gives us new values (not equal -1).
	     *		We notify waiting clients if the server has
	     *		added read-ahead data.
	     */
	    register Pdev_BufPtrs *argPtr = (Pdev_BufPtrs *)inBufPtr->addr;
	    if (inBufPtr->size != sizeof(Pdev_BufPtrs)) {
		status = GEN_INVALID_ARG;
	    } else {
		/*
		 * Verify the request buffer pointer.  The server may just
		 * be telling us about read ahead data, in which case we
		 * shouldn't muck with the request pointers. Otherwise we
		 * update the request first byte to reflect the processing
		 * of some requests by the server.
		 */
		DBG_PRINT( ("SET  %x,%x req %d:%d read %d:%d\n",
		    pdevHandlePtr->hdr.fileID.major,
		    pdevHandlePtr->hdr.fileID.minor,
		    argPtr->requestFirstByte, argPtr->requestLastByte,
		    argPtr->readFirstByte, argPtr->readLastByte) );
		if (argPtr->requestFirstByte <=
		            pdevHandlePtr->requestBuf.size &&
		    argPtr->requestFirstByte >= 0) {
		    pdevHandlePtr->requestBuf.firstByte =
			   argPtr->requestFirstByte;
		}
		Sync_Broadcast(&pdevHandlePtr->caughtUp);
	        if ((pdevHandlePtr->readBuf.data == (Address)NIL) ||
		    (argPtr->readLastByte < 0)) {
		    /*
		     * No read ahead info.
		     */
		    break;
		}
		if (argPtr->readLastByte > pdevHandlePtr->readBuf.size) {
		    printf(
			"FsServerStreamIOControl: set bad readPtr\n");
		    status = GEN_INVALID_ARG;
		    break;
		}
		if ((pdevHandlePtr->flags & PDEV_READ_BUF_EMPTY) == 0) {
		    /*
		     * Non-empty buffer.  Break out if bad pointer, else
		     * fall through to code that updates the pointer.
		     */
		    if (argPtr->readLastByte <=
			pdevHandlePtr->readBuf.lastByte) {
			    break;	/* No new read ahead data */
		    }
		} else if (pdevHandlePtr->flags & PDEV_SERVER_KNOWS_IT) {
		    /*
		     * Empty buffer and the server already knows this.
		     * We can safely reset firstByte to the beginning.
		     * The server should rely on this behavior.
		     */
		    if (argPtr->readLastByte >= 0) {
			pdevHandlePtr->flags &= ~(PDEV_READ_BUF_EMPTY|
						 PDEV_SERVER_KNOWS_IT);
			pdevHandlePtr->readBuf.firstByte = 0;
		    } else {
			break;	/* No new read ahead data */
		    }
		} else {
		    /*
		     * We emptied the buffer, but the server added data
		     * before seeing it was empty.  Can't reset firstByte.
		     */
		    if (argPtr->readLastByte > 
			    pdevHandlePtr->readBuf.lastByte) {
			pdevHandlePtr->flags &= ~PDEV_READ_BUF_EMPTY;
		    } else {
			break;	/* No new read ahead data */
		    }
		}
		/*
		 * We know here that the lastByte pointer indicates
		 * more data.  Otherwise we've broken out.
		 * Update select state and poke waiting readers.
		 */
		pdevHandlePtr->readBuf.lastByte = argPtr->readLastByte;
		pdevHandlePtr->selectBits |= FS_READABLE;
		FsFastWaitListNotify(&pdevHandlePtr->cltReadWaitList);
	    }
	    break;
	}
	case IOC_PDEV_REPLY: {
	    /*
	     * The server is replying to a request.
	     *
	     * Side effects:
	     *		Copy the reply from the server to the client.
	     *		Put the client on wait lists, if appropriate.
	     *		Notify the replyReady condition, and lastly
	     *		notify waiting clients about new select state.
	     */
	    register Pdev_Reply *srvReplyPtr = (Pdev_Reply *)inBufPtr->addr;

	    pdevHandlePtr->reply = *srvReplyPtr;
	    if (srvReplyPtr->replySize > 0) {
		register Proc_ControlBlock *clientProcPtr;

		/*
		 * Copy the reply into the waiting buffers.  PDEV_WRITE is
		 * handled specially because the reply buffer is just an
		 * integer variable in the kernel, while the input buffer
		 * is in user space, which is indicated by the FS_USER flag.
		 *  - To be fully general we'd need a user space flag for
		 * 	both the input buffer and the reply.
		 */
		if (((pdevHandlePtr->flags & FS_USER) == 0) ||
		    (pdevHandlePtr->operation == PDEV_WRITE)) {
		    status = Vm_CopyIn(srvReplyPtr->replySize,
				       srvReplyPtr->replyBuf,
				       pdevHandlePtr->replyBuf);
	        } else {
		    clientProcPtr = Proc_LockPID(pdevHandlePtr->clientPID);
		    if (clientProcPtr == (Proc_ControlBlock *)NIL) {
			status = FS_BROKEN_PIPE;
		    } else {
			status = Vm_CopyOutProc(srvReplyPtr->replySize,
				srvReplyPtr->replyBuf, FALSE,
				clientProcPtr, pdevHandlePtr->replyBuf);
			Proc_Unlock(clientProcPtr);
		    }
		}
		if (status != SUCCESS) {
		    pdevHandlePtr->flags |= PDEV_REPLY_FAILED;
		}
	    }
	    PDEV_REPLY(&pdevHandlePtr->hdr.fileID, srvReplyPtr);
	    if (srvReplyPtr->status == FS_WOULD_BLOCK) {
		if (pdevHandlePtr->operation == PDEV_READ) {
		    FsFastWaitListInsert(&pdevHandlePtr->cltReadWaitList,
					 &pdevHandlePtr->clientWait);
		} else if (pdevHandlePtr->operation == PDEV_WRITE) {
		    FsFastWaitListInsert(&pdevHandlePtr->cltWriteWaitList,
					 &pdevHandlePtr->clientWait);
		}
	    }
	    /*
	     * Wakeup the client waiting for this reply.
	     */
	    pdevHandlePtr->flags |= PDEV_REPLY_READY;
	    Sync_Broadcast(&pdevHandlePtr->replyReady);
	    /*
	     * Notify other clients that may also be blocked on the stream.
	     */
	    pdevHandlePtr->selectBits = srvReplyPtr->selectBits;
	    PdevClientNotify(pdevHandlePtr);
	    break;
	}
	case IOC_PFS_OPEN: {
	    /*
	     * A pseudo-filesystem server is replying to an open request by
	     * asking us to open a pseudo-device connection to the client.
	     * It gives us a Fs_FileID for its own identification of the
	     * connection.  The connection is set up here. A user-level
	     * streamID is generated for the server process and returned.
	     * To finish up, we set up the FsOpenResults that the waiting
	     * client has to RequestResponse.  This includes a fileID so
	     * the client process can fetch its half of the connection.
	     */
	    int newStreamID;			/* For the server */
	    FsOpenResults openResults;		/* For the client */

	    if (inBufPtr->size < sizeof(Fs_FileID) ||
		outBufPtr->size < sizeof(int)) {
		status = GEN_INVALID_ARG;
	    } else {
		newStreamID = FsPfsOpenConnection(pdevHandlePtr,
			    (Fs_FileID *)inBufPtr->addr, &openResults);
		if (outBufPtr->flags & FS_USER) {
		    Vm_CopyOut(sizeof(int), (Address)&newStreamID,
				outBufPtr->addr);
		} else {
		    *(int *)outBufPtr->addr = newStreamID;
		}
		if (newStreamID < 0) {
		    status = FAILURE;
		} else {
		    /*
		     * Here we copy the openResults to the waiting processes
		     * kernel stack (it's waiting in FsPfsOpen on this
		     * same host.)
		     */
		    register FsOpenResults *openResultsPtr =
			    (FsOpenResults *)pdevHandlePtr->replyBuf;
		    *openResultsPtr = openResults;
		}
	    }
	    if (status != SUCCESS) {
		pdevHandlePtr->flags |= PDEV_REPLY_FAILED;
		pdevHandlePtr->reply.replySize = 0;
	    } else {
		pdevHandlePtr->reply.replySize = sizeof(FsOpenResults);
	    }
	    pdevHandlePtr->reply.status = status;
	    pdevHandlePtr->flags |= PDEV_REPLY_READY;
	    Sync_Broadcast(&pdevHandlePtr->replyReady);
	    PdevClientNotify(pdevHandlePtr);
	    break;
	}
#ifdef notdef
	case IOC_PFS_SET_ID: {
	    /*
	     * A pseudo-filesystem server is setting its own notion of the
	     * fileID associated with a request-response stream.
	     */
	    register Fs_FileID *fileIDPtr;

	    if (inBufPtr->size < sizeof(Fs_FileID)) {
		status = FS_INVALID_ARG;
	    } else {
		fileIDPtr = (Fs_FileID *)inBufPtr->addr;
		pdevHandlePtr->userLevelID = *fileIDPtr;
	    }
	    break;
	}
#endif
	case IOC_PFS_PASS_STREAM:
	    /*
	     * A pseudo-filesystem server is replying to an open request by
	     * asking us to pass one of its open streams to the client.
	     */
	    break;
	case IOC_PDEV_READY:
	    /*
	     * Master has made the device ready.  The inBuffer contains
	     * new select bits.
	     *
	     * Side effects:
	     *		Notify waiting clients.
	     */

	    if (inBufPtr->size != sizeof(int)) {
		status = FS_INVALID_ARG;
	    } else {
		/*
		 * Update the select state of the pseudo-device and
		 * wake up any clients waiting on their pseudo-stream.
		 */
		pdevHandlePtr->selectBits = *(int *)inBufPtr->addr;
		PdevClientNotify(pdevHandlePtr);
	    }
	    break;
	case IOC_REPOSITION:
	    status = GEN_INVALID_ARG;
	    break;
	case IOC_GET_FLAGS:
	case IOC_SET_FLAGS:
	case IOC_SET_BITS:
	case IOC_CLEAR_BITS:
	    /*
	     * There are no server stream specific flags.
	     */
	    break;
	case IOC_TRUNCATE:
	case IOC_LOCK:
	case IOC_UNLOCK:
	case IOC_MAP:
	    status = FS_INVALID_ARG;
	    break;
	case IOC_GET_OWNER:
	case IOC_SET_OWNER:
	    status = GEN_NOT_IMPLEMENTED;
	    break;
	case IOC_NUM_READABLE: {
	    /*
	     * The server stream is readable only if there are requests in the
	     * request buffer or if the read ahead buffers have changed since
	     * the last time the server did a read.
	     *
	     * Side effects:
	     *		None.
	     */
	    register int reqFirstByte, reqLastByte;
	    register int numReadable;

	    reqFirstByte = pdevHandlePtr->requestBuf.firstByte;
	    reqLastByte = pdevHandlePtr->requestBuf.lastByte;
	    if (((pdevHandlePtr->flags & PDEV_READ_PTRS_CHANGED) == 0) &&
		((reqFirstByte == -1) || (reqFirstByte > reqLastByte))) {
		numReadable = 0;
	    } else {
		numReadable = sizeof(Pdev_BufPtrs);
	    }
	    if (outBufPtr->addr == (Address)NIL ||
		outBufPtr->size < sizeof(int)) {
		status = GEN_INVALID_ARG;
	    } else {
		*(int *)outBufPtr->addr = numReadable;
		status = SUCCESS;
	    }
	    break;
	}
	default:
	    status = GEN_NOT_IMPLEMENTED;
	    break;
    }
    if (status != SUCCESS) {
	printf("PdevServer IOControl #%x returning %x\n", command, status);
    }
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsServerStreamClose --
 *
 *	Clean up the state associated with a server stream.  This makes
 *	sure the client processes associated with the pseudo stream get
 *	poked, and it marks the pseudo stream's state as invalid so
 *	the clients will abort their current operations, if any.  The
 *	handle is 'removed' here, but it won't go away until the client
 *	side closes down and releases its reference to it.
 *
 * Results:
 *	SUCCESS.
 *
 * Side effects:
 *	Marks the pseudo stream state with PDEV_SERVER_GONE, notifies
 *	all conditions in pseudo stream state, wakes up all processes
 *	in any of the pseudo stream's wait lists, and then removes
 *	the handle from the hash table.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
FsServerStreamClose(streamPtr, clientID, procID, flags, size, data)
    Fs_Stream		*streamPtr;	/* Service stream to close */
    int			clientID;	/* HostID of client closing */
    Proc_PID		procID;		/* ID of closing process */
    int			flags;		/* Flags from the stream being closed */
    int			size;		/* Should be zero */
    ClientData		data;		/* IGNORED */
{
    register PdevServerIOHandle *pdevHandlePtr =
	    (PdevServerIOHandle *)streamPtr->ioHandlePtr;

    DBG_PRINT( ("Server Closing pdev %x,%x\n", 
		pdevHandlePtr->hdr.fileID.major,
		pdevHandlePtr->hdr.fileID.minor) );

    PdevClientWakeup(pdevHandlePtr);
    if (pdevHandlePtr->ctrlHandlePtr->prefixPtr != (FsPrefix *)NIL) {
	/*
	 * This is the naming requeust-response stream of a pseudo-filesystem.
	 */
	register PdevControlIOHandle *ctrlHandlePtr;
	Fs_Stream dummy;

	ctrlHandlePtr = pdevHandlePtr->ctrlHandlePtr;
	dummy.hdr.fileID.type = -1;
	dummy.ioHandlePtr = (FsHandleHeader *)ctrlHandlePtr;
	FsHandleLock(ctrlHandlePtr);
	FsPrefixHandleClose(ctrlHandlePtr->prefixPtr);
	(void)FsControlClose(&dummy, clientID, procID, flags, 0, (ClientData)NIL);
    }
    FsHandleRelease(pdevHandlePtr, TRUE);
    FsHandleRemove(pdevHandlePtr);	/* No need for scavenging */
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * PdevClientWakeup --
 *
 *	Called when the server's stream is closed.  This
 *	notifies the various conditions that the client might be
 *	waiting on and marks the pdev state as invalid so the
 *	client will bail out when it wakes up.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Notifies condition variables and marks the pseudo stream as invalid.
 *
 *----------------------------------------------------------------------
 */

ENTRY static void
PdevClientWakeup(pdevHandlePtr)
    PdevServerIOHandle *pdevHandlePtr;	/* State for the pseudo stream */
{
    LOCK_MONITOR;
    /*
     * Set both "busy" and "server gone" because the standard preamble
     * for all client routines only checks against "server gone"
     * inside a while-not-busy loop.
     */
    pdevHandlePtr->flags |= (PDEV_SERVER_GONE|PDEV_REPLY_FAILED|PDEV_BUSY);
    Sync_Broadcast(&pdevHandlePtr->access);
    Sync_Broadcast(&pdevHandlePtr->caughtUp);
    Sync_Broadcast(&pdevHandlePtr->replyReady);
    FsFastWaitListNotify(&pdevHandlePtr->cltReadWaitList);
    FsFastWaitListNotify(&pdevHandlePtr->cltWriteWaitList);
    FsFastWaitListNotify(&pdevHandlePtr->cltExceptWaitList);
    UNLOCK_MONITOR;
}

/*
 *----------------------------------------------------------------------
 *
 * FsPdevServerOK --
 *
 *	Called from FsPfsExport to see if the server of a prefix still exists.
 *
 * Results:
 *	TRUE if the server process is still around.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ENTRY Boolean
FsPdevServerOK(pdevHandlePtr)
    PdevServerIOHandle *pdevHandlePtr;	/* State for the pseudo stream */
{
    register Boolean answer;
    LOCK_MONITOR;
    if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	answer = FALSE;
    } else {
	answer = TRUE;
    }
    UNLOCK_MONITOR;
    return(answer);
}


/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamOpen --
 *
 *	Do the first request-response with a pseudo-device server to see if it
 *	will accept the open by the client.   This is used to tell
 *	the server something about the new stream it is getting,
 *	and to let it decide if it will accept the open.
 *
 * Results:
 *	The return status from the server's reply.  It can reject the open.
 *
 * Side effects:
 *	An PDEV_OPEN request-response is carried out.
 *
 *----------------------------------------------------------------------
 */

ENTRY ReturnStatus
FsPseudoStreamOpen(pdevHandlePtr, flags, clientID, procID, userID)
    register PdevServerIOHandle *pdevHandlePtr;	/* Pdev connection state */
    int		flags;		/* Open flags */
    int		clientID;	/* Host ID of the client */
    Proc_PID	procID;		/* Process ID of the client process */
    int		userID;		/* User ID of the client process */
{
    register ReturnStatus 	status;
    Pdev_Request		request;

    LOCK_MONITOR;

    /*
     * Wait for the server to set up the request buffer.  The state starts
     * out PDEV_BUSY to eliminate an explicit check for PDEV_SETUP in all
     * the routines that call RequestResponse.  PDEV_BUSY is cleared after
     * the server initializes the request buffer with IOC_PDEV_SETUP.
     */
    while (pdevHandlePtr->flags & PDEV_BUSY) { 
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = DEV_OFFLINE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;

    /*
     * Issue the first request to the server to see if it will accept us.
     */

    request.hdr.operation	= PDEV_OPEN;
    request.param.open.flags	= flags;
    request.param.open.pid	= procID;
    request.param.open.hostID	= clientID;
    request.param.open.uid	= userID;

    pdevHandlePtr->flags &= ~FS_USER;
    status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request), &request.hdr,
			 0, (Address) NIL, 0, (Address) NIL, (int *)NIL,
			 (Sync_RemoteWaiter *)NIL);
    pdevHandlePtr->flags &= ~PDEV_BUSY;
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamLookup --
 *
 *	Do a lookup request-response to a pseudo-filesystem server over
 *	the naming request-response stream that is hooked to the prefix table.
 *	If a pathname redirection occurs this allocates a buffer for the
 *	returned pathname.  Otherwise this is a stub that passes its arguments
 *	up to the user level pseudo-filesystem server via request-response.
 *
 * Results:
 *	A status and some bundled results that aren't interpreted by us.
 *	If status is FS_REDIRECT then *newNameInfoPtrPtr has been allocated
 *	and contains the returned pathname.
 *
 * Side effects:
 *	The effect of the naming operation in the pseudo-filesystem is
 *	up to the pseudo-filesystem server.  Upon FS_REDIRECT this
 *	allocates the *newNameInfoPtrPtr buffer.
 *
 *----------------------------------------------------------------------
 */

ENTRY ReturnStatus
FsPseudoStreamLookup(pdevHandlePtr, requestPtr, argSize, argsPtr,
		    resultsSizePtr, resultsPtr, newNameInfoPtrPtr)
    register PdevServerIOHandle *pdevHandlePtr;	/* Pdev connection state */
    Pfs_Request		*requestPtr;	/* Semi-initialized request header */
    int			argSize;	/* Size of argsPtr buffer */
    Address		argsPtr;	/* Ref. to bundled args, usually name */
    int			*resultsSizePtr;/* In/Out size of results */
    Address		resultsPtr;	/* Ref. to bundled results or 
					 * FsRedirectInfo */
    FsRedirectInfo	**newNameInfoPtrPtr;/* Set if server returns name */
{
    register ReturnStatus 	status;
    FsRedirectInfo		redirectInfo;

    LOCK_MONITOR;

    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = FS_STALE_HANDLE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;

    pdevHandlePtr->flags &= ~FS_USER;
    status = RequestResponse(pdevHandlePtr, sizeof(Pfs_Request),
		&requestPtr->hdr, argSize, argsPtr, sizeof(FsRedirectInfo),
		(Address)&redirectInfo, resultsSizePtr,
		(Sync_RemoteWaiter *)NIL);
    if (status == FS_LOOKUP_REDIRECT) {
	*newNameInfoPtrPtr = mnew(FsRedirectInfo);
	(*newNameInfoPtrPtr)->prefixLength = redirectInfo.prefixLength;
	(void)strcpy((*newNameInfoPtrPtr)->fileName, redirectInfo.fileName);
    } else {
	*newNameInfoPtrPtr = (FsRedirectInfo *)NIL;
	if (*resultsSizePtr > 0) {
	    bcopy((Address)&redirectInfo, resultsPtr, *resultsSizePtr);
	}
    }

    if (status == DEV_OFFLINE) {
	/*
	 * Return stale handle so remote clients know to nuke
	 * their prefix table entry.
	 */
	status = FS_STALE_HANDLE;
    }
    pdevHandlePtr->flags &= ~PDEV_BUSY;
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStream2Path --
 *
 *	Do a rename/link request-response to a pseudo-filesystem server over
 *	the naming request-response stream that is hooked to the prefix table.
 *	If a pathname redirection occurs this allocates a buffer for the
 *	returned pathname.  Otherwise this is a stub that passes its arguments
 *	up to the user level pseudo-filesystem server via request-response.
 *
 * Results:
 *	A status and some bundled results that aren't interpreted by us.
 *	If status is FS_REDIRECT then *newNameInfoPtrPtr has been allocated
 *	and contains the returned pathname.
 *
 * Side effects:
 *	The effect of the naming operation in the pseudo-filesystem is
 *	up to the pseudo-filesystem server.  Upon FS_REDIRECT this
 *	allocates the *newNameInfoPtrPtr buffer.
 *
 *----------------------------------------------------------------------
 */

ENTRY ReturnStatus
FsPseudoStream2Path(pdevHandlePtr, requestPtr, dataPtr, name1ErrorPtr,
		    newNameInfoPtrPtr)
    register PdevServerIOHandle *pdevHandlePtr;	/* Pdev connection state */
    Pfs_Request		*requestPtr;	/* Semi-initialized request header */
    Fs2PathData		*dataPtr;	/* 2 pathnames */
    Boolean		*name1ErrorPtr;	/* TRUE if error applies to first path*/
    FsRedirectInfo	**newNameInfoPtrPtr;/* Set if server returns name */
{
    register ReturnStatus 	status;
    int resultSize;
    Fs2PathRedirectInfo		redirectInfo;

    LOCK_MONITOR;

    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = FS_STALE_HANDLE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;

    pdevHandlePtr->flags &= ~FS_USER;
    status = RequestResponse(pdevHandlePtr, sizeof(Pfs_Request),
		&requestPtr->hdr, sizeof(Fs2PathData), (Address)dataPtr,
		sizeof(Fs2PathRedirectInfo), (Address)&redirectInfo,
		&resultSize, (Sync_RemoteWaiter *)NIL);
    if (status == FS_LOOKUP_REDIRECT) {
	*newNameInfoPtrPtr = mnew(FsRedirectInfo);
	(*newNameInfoPtrPtr)->prefixLength = redirectInfo.prefixLength;
	(void)strcpy((*newNameInfoPtrPtr)->fileName, redirectInfo.fileName);
    }
    if (status != SUCCESS) {
	*name1ErrorPtr = redirectInfo.name1ErrorP;
    } else {
	*name1ErrorPtr = FALSE;
    }

    if (status == DEV_OFFLINE) {
	/*
	 * Return stale handle so remote clients know to nuke
	 * their prefix table entry.
	 */
	status = FS_STALE_HANDLE;
    }
    pdevHandlePtr->flags &= ~PDEV_BUSY;
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoGetAttr --
 *
 *	Called from Fs_GetAttributesID to get the attributes of a file
 *	in a pseudo-filesystem.  The stream's nameInfoPtr->fileID is
 *	set to the fileID of the pseudo-device connection to the server,
 *	and this ID is passed into us.  This does a PDEV_GET_ATTR request.
 *
 * Results:
 *	The attributes structure is passed back from the pfs server.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
FsPseudoGetAttr(fileIDPtr, clientID, attrPtr)
    register Fs_FileID		*fileIDPtr;	/* Identfies pdev connection */
    int				clientID;	/* Host ID of process asking
						 * for the attributes */
    register Fs_Attributes	*attrPtr;	/* Return - the attributes */
{
    PdevClientIOHandle		*cltHandlePtr;
    Pdev_Request		request;
    register PdevServerIOHandle	*pdevHandlePtr;
    register ReturnStatus	status;

    cltHandlePtr = FsHandleFetchType(PdevClientIOHandle, fileIDPtr);
    if (cltHandlePtr == (PdevClientIOHandle *)NIL) {
	printf( "FsPseudoGetAttr, no %s handle <%d,%x,%x>\n",
	    FsFileTypeToString(fileIDPtr->type), fileIDPtr->serverID,
	    fileIDPtr->major, fileIDPtr->minor);
	return(FS_FILE_NOT_FOUND);
    }
    FsHandleRelease(cltHandlePtr, TRUE);
    pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    LOCK_MONITOR;

    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = DEV_OFFLINE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;
    pdevHandlePtr->flags &= ~FS_USER;
    request.hdr.operation = PDEV_GET_ATTR;
    status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request), &request.hdr,
			0, (Address) NIL,
			sizeof(Fs_Attributes), (Address)attrPtr,
			(int *)NIL, (Sync_RemoteWaiter *)NIL);
    pdevHandlePtr->flags &= ~PDEV_BUSY;
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoSetAttr --
 *
 *	Set the attributes of a file in a pseudo-filesystem.  We have the
 *	fileID of the request-response stream to the server.  This issues
 *	a PDEV_SET_ATTR request to the server that contains new attributes.
 *
 * Results:
 *	An error code.
 *
 * Side effects:
 *	None here.  The new attributes are shipped to the pfs server.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
FsPseudoSetAttr(fileIDPtr, attrPtr, idPtr, flags)
    register Fs_FileID		*fileIDPtr;	/* Identfies pdev connection */
    register Fs_Attributes	*attrPtr;	/* Return - the attributes */
    Fs_UserIDs			*idPtr;		/* Identfies user */
    int				flags;		/* Tells which attrs to set */
{
    PdevClientIOHandle		*cltHandlePtr;
    Pdev_Request		request;
    register PdevServerIOHandle	*pdevHandlePtr;
    register ReturnStatus	status;

    cltHandlePtr = FsHandleFetchType(PdevClientIOHandle, fileIDPtr);
    if (cltHandlePtr == (PdevClientIOHandle *)NIL) {
	printf( "FsPseudoSetAttr, no handle <%d,%d,%x,%x>\n",
	    fileIDPtr->serverID, fileIDPtr->type,
	    fileIDPtr->major, fileIDPtr->minor);
	return(FS_FILE_NOT_FOUND);
    }
    FsHandleRelease(cltHandlePtr, TRUE);
    pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    LOCK_MONITOR;

    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = DEV_OFFLINE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;
    pdevHandlePtr->flags &= ~FS_USER;
    request.hdr.operation = PDEV_SET_ATTR;
    request.param.setAttr.uid = idPtr->user;
    request.param.setAttr.flags = flags;
    status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request), &request.hdr,
			sizeof(Fs_Attributes), (Address)attrPtr,
			0, (Address) NIL,
			(int *)NIL, (Sync_RemoteWaiter *)NIL);
    pdevHandlePtr->flags &= ~PDEV_BUSY;
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamGetIOAttr --
 *
 *	Called from Fs_GetAttrStream to get the I/O attributes of a
 *	pseudo-device.  The access and modify times of the pseudo-device
 *	are obtained from the internal pdev state.
 *
 * Results:
 *	SUCCESS.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
FsPseudoStreamGetIOAttr(fileIDPtr, clientID, attrPtr)
    register Fs_FileID		*fileIDPtr;	/* Identfies pdev connection */
    int				clientID;	/* Host ID of process asking
						 * for the attributes */
    register Fs_Attributes	*attrPtr;	/* Return - the attributes */
{
    PdevClientIOHandle		*cltHandlePtr;
    register PdevServerIOHandle	*pdevHandlePtr;

    cltHandlePtr = FsHandleFetchType(PdevClientIOHandle, fileIDPtr);
    if (cltHandlePtr == (PdevClientIOHandle *)NIL) {
	printf( "FsPseudoStreamGetIOAttr, no %s handle <%d,%x,%x> client %d\n",
	    FsFileTypeToString(fileIDPtr->type), fileIDPtr->serverID,
	    fileIDPtr->major, fileIDPtr->minor, clientID);
	return(FS_FILE_NOT_FOUND);
    }
    FsHandleRelease(cltHandlePtr, TRUE);
    pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    LOCK_MONITOR;

    attrPtr->accessTime.seconds = pdevHandlePtr->ctrlHandlePtr->accessTime;
    attrPtr->dataModifyTime.seconds = pdevHandlePtr->ctrlHandlePtr->modifyTime;

    UNLOCK_MONITOR;
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamSetIOAttr --
 *
 *	Set the IO attributes of a pseudo-device.
 *
 * Results:
 *	An error code.
 *
 * Side effects:
 *	Updates the access and modify times kept in the pdev state.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
FsPseudoStreamSetIOAttr(fileIDPtr, attrPtr, flags)
    register Fs_FileID		*fileIDPtr;	/* Identfies pdev connection */
    register Fs_Attributes	*attrPtr;	/* Return - the attributes */
    int				flags;		/* Tells which attrs to set */
{
    PdevClientIOHandle		*cltHandlePtr;
    register PdevServerIOHandle	*pdevHandlePtr;

    cltHandlePtr = FsHandleFetchType(PdevClientIOHandle, fileIDPtr);
    if (cltHandlePtr == (PdevClientIOHandle *)NIL) {
	printf( "FsPseudoStreamSetIOAttr, no handle <%d,%d,%x,%x>\n",
	    fileIDPtr->serverID, fileIDPtr->type,
	    fileIDPtr->major, fileIDPtr->minor);
	return(FS_FILE_NOT_FOUND);
    }
    FsHandleRelease(cltHandlePtr, TRUE);
    pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    LOCK_MONITOR;
    if (flags & FS_SET_TIMES) {
	pdevHandlePtr->ctrlHandlePtr->accessTime = attrPtr->accessTime.seconds;
	pdevHandlePtr->ctrlHandlePtr->modifyTime = attrPtr->dataModifyTime.seconds;
    }
    UNLOCK_MONITOR;
    return(SUCCESS);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamRead --
 *
 *	Read from a pseudo-device stream. If there is data in the read
 *	ahead buffer (if one exists), that is used to satisfy the read.
 *	Otherwise a request-response exchange with the server is used
 *	to do the read.
 *
 * Results:
 *	SUCCESS, and *lenPtr reflects how much was read.  When the server
 *	goes away EOF is simulated by a SUCCESS return and *lenPtr == 0.
 *	If there is no data in the read ahead buffer FS_WOULD_BLOCK is returned.
 *
 * Side effects:
 *	If applicable, pointers into the read ahead buffer are adjusted.
 *	The buffer is filled with the number of bytes indicated by
 *	the length parameter.  The in/out length parameter specifies
 *	the buffer size on input and is updated to reflect the number
 *	of bytes actually read.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
FsPseudoStreamRead(streamPtr, flags, buffer, offsetPtr, lenPtr, waitPtr)
    register Fs_Stream 	*streamPtr;	/* Stream to read from. */
    Address 	buffer;			/* Where to read into. */
    int		*offsetPtr;		/* In/Out byte offset for the read */
    int 	*lenPtr;		/* In/Out byte count parameter */
    Sync_RemoteWaiter *waitPtr;		/* Process info for waiting */
{
    ReturnStatus 	status;
    register PdevClientIOHandle *cltHandlePtr =
	    (PdevClientIOHandle *)streamPtr->ioHandlePtr;
    register PdevServerIOHandle *pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    Pdev_Request	request;

    LOCK_MONITOR;
    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = DEV_OFFLINE;
	    goto exitNoServer;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;

    if (pdevHandlePtr->readBuf.data != (Address)NIL) {
	/*
	 * A read ahead buffer exists so we get data from it.  If it's
	 * empty we put the client on the client I/O handle read wait list.
	 */
	if (pdevHandlePtr->flags & PDEV_READ_BUF_EMPTY) {
	    status = FS_WOULD_BLOCK;
	    FsFastWaitListInsert(&pdevHandlePtr->cltReadWaitList, waitPtr);
	    PDEV_TRACE(&pdevHandlePtr->hdr.fileID, PDEVT_READ_WAIT);
	    DBG_PRINT( ("PDEV %x,%x Read (%d) Blocked\n", 
		    streamPtr->ioHandlePtr->fileID.major,
		    streamPtr->ioHandlePtr->fileID.minor,
		    *lenPtr) );
	    *lenPtr = 0;
	} else {
	    register int dataAvail, firstByte, lastByte, toRead;
	    register Proc_ControlBlock *serverProcPtr;

	    firstByte = pdevHandlePtr->readBuf.firstByte;
	    lastByte = pdevHandlePtr->readBuf.lastByte;
	    dataAvail = lastByte - firstByte + 1;
	    if (dataAvail <= 0) {
		panic( 
		    "FsPseudoStreamRead, dataAvail in read buf <= 0 bytes\n");
		status = DEV_OFFLINE;
		goto exit;
	    }
	    /*
	     * Lock down the server process in preparation for copying
	     * from the read ahead buffer.
	     */
	    serverProcPtr = Proc_LockPID(pdevHandlePtr->serverPID);
	    if (serverProcPtr == (Proc_ControlBlock *)NIL) {
		status = DEV_OFFLINE;
		goto exit;
	    }
	    /*
	     * Decide how much to read and note if we empty the buffer.
	     */
	    if (dataAvail > *lenPtr) {
		toRead = *lenPtr;
	    } else {
		toRead = dataAvail;
		pdevHandlePtr->flags |= PDEV_READ_BUF_EMPTY;
	    }
	    DBG_PRINT( ("PDEV %x,%x Read %d Avail %d\n", 
		    pdevHandlePtr->hdr.fileID.major,
		    pdevHandlePtr->hdr.fileID.minor,
		    toRead, dataAvail) );
	    /*
	     * Copy out of the read ahead buffer to the client's buffer.
	     */
	    status = Vm_CopyInProc(toRead, serverProcPtr,
			  pdevHandlePtr->readBuf.data + firstByte,
			  buffer, (flags & FS_USER) == 0);
	    Proc_Unlock(serverProcPtr);
	    /*
	     * Update pointers and poke the server so it can find out.
	     */
	    *lenPtr = toRead;
	    firstByte += toRead;
	    pdevHandlePtr->readBuf.firstByte = firstByte;
	    pdevHandlePtr->flags |= PDEV_READ_PTRS_CHANGED;
	    FsFastWaitListNotify(&pdevHandlePtr->srvReadWaitList);
	}
    } else if (pdevHandlePtr->selectBits & FS_READABLE) {
	Proc_ControlBlock	*procPtr;

	/*
	 * No read ahead buffer and the state is readable.
	 * Set up and do the request-response exchange.
	 */
	procPtr = Proc_GetEffectiveProc();
	request.hdr.operation		= PDEV_READ;
	request.param.read.offset	= *offsetPtr;
	request.param.read.familyID	= procPtr->familyID;
	request.param.read.procID	= procPtr->processID;

	pdevHandlePtr->flags |= (flags & FS_USER);
	status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request),
	    &request.hdr, 0, (Address) NIL, *lenPtr, buffer, lenPtr, waitPtr);
    } else {
	/*
	 * The pseudo-device is not readable now.
	 */
	FsFastWaitListInsert(&pdevHandlePtr->cltReadWaitList, waitPtr);
	*lenPtr = 0;
	status = FS_WOULD_BLOCK;
    }
    *offsetPtr += *lenPtr;
    pdevHandlePtr->ctrlHandlePtr->accessTime = fsTimeInSeconds;
exit:
    if (status == DEV_OFFLINE) {
	/*
	 * Simulate EOF
	 */
	status = SUCCESS;
	*lenPtr = 0;
    }
    pdevHandlePtr->flags &= ~(PDEV_BUSY|FS_USER);
exitNoServer:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamWrite --
 *
 *	Write to a pseudo-device by a client.  The write is done
 *	asynchronously if the stream allows that.  In that case we
 *	always say we could write as much as requested.  However, if
 *	the write is larger than the server's request buffer we
 *	break it into blocks that each fit. To support UDP, the
 *	stream can also be marked to dis-allow these large writes.
 *	Finally, the stream may be marked synchronous in which case
 *	we tell RequestResponse to wait for a reply.
 *
 * Results:
 *	SUCCESS			- the data was written.
 *
 * Side effects:
 *	The data in the buffer is written to the device.  Large writes
 *	are broken into a series of shorter writes, although we keep
 *	the access lock on the pseudo-stream so the whole write completes.
 *	The in/out length parameter specifies the amount of data to write
 *	and is updated to reflect the number of bytes actually written.
 *
 *----------------------------------------------------------------------
 */
ENTRY ReturnStatus
FsPseudoStreamWrite(streamPtr, flags, buffer, offsetPtr, lenPtr, waitPtr)
    Fs_Stream 	*streamPtr;	/* Stream to write to. */
    int		flags;		/* Flags from the stream */
    Address 	buffer;		/* Where to write to. */
    int		*offsetPtr;	/* In/Out byte offset */
    int 	*lenPtr;	/* In/Out byte count */
    Sync_RemoteWaiter *waitPtr;	/* Process info for waiting on I/O */
{
    register Proc_ControlBlock *procPtr;
    register PdevClientIOHandle *cltHandlePtr =
	    (PdevClientIOHandle *)streamPtr->ioHandlePtr;
    register PdevServerIOHandle *pdevHandlePtr = cltHandlePtr->pdevHandlePtr;
    ReturnStatus 	status = SUCCESS;
    Pdev_Request	request;
    register int	toWrite;
    int			amountWritten;
    register int	length;
    int			replySize;
    int			numBytes;
    int			maxRequestSize;

    LOCK_MONITOR;
    /*
     * Wait for exclusive access to the stream.
     */
    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = FS_BROKEN_PIPE;
	    goto exitNoServer;
	}
    }
    pdevHandlePtr->flags |= (PDEV_BUSY|(flags & FS_USER));

    /*
     * Allow flow control by checking the select bits and only trying
     * the operation if the state allows it.
     */
    if ((pdevHandlePtr->selectBits & FS_WRITABLE) == 0) {
	*lenPtr = 0;
	FsFastWaitListInsert(&pdevHandlePtr->cltWriteWaitList, waitPtr);
	status = FS_WOULD_BLOCK;
	goto exit;
    }
    /*
     * The write request parameters are the offset and flags parameters.
     * The buffer contains the data to write.
     */
    procPtr = Proc_GetEffectiveProc();
    if (pdevHandlePtr->flags & PDEV_WRITE_BEHIND) {
	request.hdr.operation		= PDEV_WRITE_ASYNC;
    } else {
	request.hdr.operation		= PDEV_WRITE;
    }
    request.param.write.offset		= *offsetPtr;
    request.param.write.familyID	= procPtr->familyID;
    request.param.write.procID		= procPtr->processID;

    toWrite = *lenPtr;
    amountWritten = 0;
    maxRequestSize = pdevHandlePtr->requestBuf.size - sizeof(Pdev_Request);
    if (toWrite > maxRequestSize &&
	(pdevHandlePtr->flags & PDEV_NO_BIG_WRITES)) {
	printf(
	    "Too large a write (%d bytes) attempted on pseudo-device (UDP?)\n",
	    toWrite);
	status = GEN_INVALID_ARG;
	goto exit;
    }
    while ((toWrite > 0) && (status == SUCCESS)) {
	/*
	 * Loop to put the maximum amount of data into the request
	 * buffer until the whole block has been transferred.
	 */
	if (toWrite > maxRequestSize) {
	    length = maxRequestSize;
	} else {
	    length = toWrite;
	}
	replySize = sizeof(int);
	status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request),
				 &request.hdr, length, buffer,
				 replySize, (Address)&numBytes, &replySize,
				 waitPtr);
	if (pdevHandlePtr->flags & PDEV_WRITE_BEHIND) {
	    /*
	     * Assume all bytes accepted when write-behind is enabled.
	     */
	    numBytes = length;
	} else if (replySize != sizeof(int)) {
	    printf("Pdev_Write, no return amtWritten (%s)\n",
		    FsHandleName(pdevHandlePtr));
	    numBytes = 0;
	}
	amountWritten += numBytes;
	toWrite -= numBytes;
	request.param.write.offset += numBytes;
	buffer += numBytes;
    }
    *lenPtr = amountWritten;
    *offsetPtr += amountWritten;
    pdevHandlePtr->ctrlHandlePtr->modifyTime = fsTimeInSeconds;
exit:
    if (status == DEV_OFFLINE) {
	/*
	 * Simulate a broken pipe so writers die.
	 */
	status = FS_BROKEN_PIPE;
    }
    pdevHandlePtr->flags &= ~(PDEV_BUSY|FS_USER);
exitNoServer:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamIOControl --
 *
 *	IOControls for pseudo-device.  The in parameter block of the
 *	IOControl is passed to the server, and its response is used
 *	to fill the return parameter block of the client.
 *
 * Results:
 *	SUCCESS			- the operation was successful.
 *
 * Side effects:
 *	None here in the kernel, anyway.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
FsPseudoStreamIOControl(streamPtr, command, byteOrder, inBufPtr, outBufPtr)
    Fs_Stream	*streamPtr;	/* Stream to pseudo-device */
    int		command;	/* The control operation to be performed. */
    int		byteOrder;	/* Client's byte order */
    Fs_Buffer   *inBufPtr;	/* Command inputs */
    Fs_Buffer	*outBufPtr;	/* Buffer for return parameters */
{
    ReturnStatus 	status;
    Pdev_Request	request;
    register Proc_ControlBlock *procPtr;
    register PdevClientIOHandle *cltHandlePtr =
	    (PdevClientIOHandle *)streamPtr->ioHandlePtr;
    register PdevServerIOHandle *pdevHandlePtr = cltHandlePtr->pdevHandlePtr;

    LOCK_MONITOR;
    /*
     * Wait for exclusive access to the stream.
     */
    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    status = DEV_OFFLINE;
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;

    /*
     * Decide if the buffers are already in the kernel or not.  The buffers
     * for generic I/O controls are copied in, and if we are being called
     * from an RPC stub they are also in kernel space.
     */
    if ((command > IOC_GENERIC_LIMIT) &&
	(inBufPtr->flags & FS_USER) != 0) {
	pdevHandlePtr->flags |= FS_USER;

    }

    switch (command) {
	/*
	 * Trap out the IOC_NUM_READABLE here if there's a read ahead buf.
	 */
	case IOC_NUM_READABLE: {
	    if (pdevHandlePtr->readBuf.data != (Address)NIL) {
		int bytesAvail;
		if (pdevHandlePtr->flags & PDEV_READ_BUF_EMPTY) {
		    bytesAvail = 0;
		} else {
		    bytesAvail = pdevHandlePtr->readBuf.lastByte -
				 pdevHandlePtr->readBuf.firstByte + 1;
		}
		status = SUCCESS;
		if (byteOrder != mach_ByteOrder) {
		    int size = sizeof(int);
		    Swap_Buffer((Address)&bytesAvail, sizeof(int),
			mach_ByteOrder, byteOrder, "w", outBufPtr->addr, &size);
		    if (size != sizeof(int)) {
			status = GEN_INVALID_ARG;
		    }
		} else if (outBufPtr->size != sizeof(int)) {
		    status = GEN_INVALID_ARG;
		} else {
		    *(int *)outBufPtr->addr = bytesAvail;
		}
		DBG_PRINT( ("IOC  %x,%x num readable %d\n",
			pdevHandlePtr->hdr.fileID.major,
			pdevHandlePtr->hdr.fileID.minor,
			bytesAvail) );
		break;
	    }
	    /*
	     * FALL through to request-response if no read-ahead buffer
	     */
	}
	default: {
	    procPtr = Proc_GetEffectiveProc();
	    request.hdr.operation		= PDEV_IOCTL;
	    request.param.ioctl.command		= command;
	    request.param.ioctl.familyID	= procPtr->familyID;
	    request.param.ioctl.procID		= procPtr->processID;
	    request.param.ioctl.byteOrder	= byteOrder;
	
	    status = RequestResponse(pdevHandlePtr, sizeof(Pdev_Request),
				     &request.hdr,
				     inBufPtr->size, inBufPtr->addr,
				     outBufPtr->size, outBufPtr->addr,
				     (int *) NIL, (Sync_RemoteWaiter *)NIL);
	}
    }

    pdevHandlePtr->flags &= ~(PDEV_BUSY|FS_USER);
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
    return(status);
}


/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamSelect --
 *
 *	Select on a pseudo-device.  This is done by checking the stream
 *	state kept on the host of the server.  If the pseudo device isn't
 *	selectable we just return and let the server's IOC_PDEV_READY
 *	IOControl do the wakeup for us. (ie. we don't use the handle wait
 *	lists.)
 *
 * Results:
 *	SUCCESS	or FS_WOULD_BLOCK
 *
 * Side effects:
 *	*outFlagsPtr modified to indicate whether the device is
 *	readable or writable.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
FsPseudoStreamSelect(hdrPtr, waitPtr, readPtr, writePtr, exceptPtr)
    FsHandleHeader	*hdrPtr;	/* Handle on pdev to select */
    Sync_RemoteWaiter	*waitPtr;	/* Process info for remote waiting */
    int 		*readPtr;	/* Bit to clear if non-readable */
    int 		*writePtr;	/* Bit to clear if non-writeable */
    int 		*exceptPtr;	/* Bit to clear if non-exceptable */
{
    ReturnStatus status;
    register PdevClientIOHandle *cltHandlePtr = (PdevClientIOHandle *)hdrPtr;
    register PdevServerIOHandle *pdevHandlePtr = cltHandlePtr->pdevHandlePtr;

    LOCK_MONITOR;

    PDEV_TSELECT(&cltHandlePtr->hdr.fileID, *readPtr, *writePtr, *exceptPtr);

    if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) ||
	(pdevHandlePtr->flags & PDEV_SETUP) == 0) {
	status = DEV_OFFLINE;
    } else {
	if (*readPtr) {
	    if (((pdevHandlePtr->readBuf.data == (Address)NIL) &&
		 ((pdevHandlePtr->selectBits & FS_READABLE) == 0)) ||
		((pdevHandlePtr->readBuf.data != (Address)NIL) &&
		 (pdevHandlePtr->flags & PDEV_READ_BUF_EMPTY))) {
		*readPtr = 0;
		FsFastWaitListInsert(&pdevHandlePtr->cltReadWaitList, waitPtr);
	    }
	}
	if (*writePtr && ((pdevHandlePtr->selectBits & FS_WRITABLE) == 0)) {
	    *writePtr = 0;
	    FsFastWaitListInsert(&pdevHandlePtr->cltWriteWaitList, waitPtr);
	}    
	if (*exceptPtr && ((pdevHandlePtr->selectBits & FS_EXCEPTION) == 0)) {
            *exceptPtr = 0;
	    FsFastWaitListInsert(&pdevHandlePtr->cltExceptWaitList, waitPtr);
	}
	status = SUCCESS;
    }
    UNLOCK_MONITOR;
    return(status);
}


/*
 *----------------------------------------------------------------------
 *
 * PdevClientNotify --
 *
 *	Wakeup any processes selecting or blocking on the pseudo-stream.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

INTERNAL static void
PdevClientNotify(pdevHandlePtr)
    PdevServerIOHandle *pdevHandlePtr;
{
    register int selectBits = pdevHandlePtr->selectBits;

    PDEV_WAKEUP(&pdevHandlePtr->hdr.fileID, pdevHandlePtr->clientPID,
		pdevHandlePtr->selectBits);
    if (selectBits & FS_READABLE) {
	FsFastWaitListNotify(&pdevHandlePtr->cltReadWaitList);
    }
    if (selectBits & FS_WRITABLE) {
	FsFastWaitListNotify(&pdevHandlePtr->cltWriteWaitList);
    }
    if (selectBits & FS_EXCEPTION) {
	FsFastWaitListNotify(&pdevHandlePtr->cltExceptWaitList);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FsPseudoStreamCloseInt --
 *
 *	Do a close request-response with the server.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ENTRY void
FsPseudoStreamCloseInt(pdevHandlePtr)
    register PdevServerIOHandle *pdevHandlePtr;
{
    Pdev_Request	request;

    LOCK_MONITOR;

    while (pdevHandlePtr->flags & PDEV_BUSY) {
	if ((pdevHandlePtr->flags & PDEV_SERVER_GONE) == 0) {
	    (void)Sync_Wait(&pdevHandlePtr->access, FALSE);
	}
	if (pdevHandlePtr->flags & PDEV_SERVER_GONE) {
	    goto exit;
	}
    }
    pdevHandlePtr->flags |= PDEV_BUSY;
    /*
     * Someday we could set up a timeout call-back here in case
     * the server never replies.
     */
    request.hdr.operation = PDEV_CLOSE;
    (void) RequestResponse(pdevHandlePtr, sizeof(Pdev_Request), &request.hdr,
		    0, (Address)NIL, 0, (Address)NIL, (int *) NIL,
		    (Sync_RemoteWaiter *)NIL);
    pdevHandlePtr->flags &= ~(PDEV_BUSY|FS_USER);
exit:
    Sync_Broadcast(&pdevHandlePtr->access);
    UNLOCK_MONITOR;
}
