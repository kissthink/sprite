/*
 * pdev.h --
 *
 *	Declarations of the kernel/user-level-server interface for
 *	pseudo-devices. A pseudo-device is a file
 *	whose semantics are implemented by a user-level server process.
 *	All other processes act on the file normally.  Their operations
 *	on the file are mapped by the kernel into a request-response
 *	exchange with the server process.  The types and constants defined
 *	here are needed by the server programs to implement their
 *	half of the pseudo-device protocol.  As well as implement the
 *	regular file operations, Fs_Read, Fs_Write, and Fs_IOControl,
 *	the server is involved in open/close time actions, and the
 *	server helps control the select state of the pseudo device.
 *	
 *	A more complete explaination of pseudo-devices should be in
 *	the man page (/sprite/doc/ref/devices/pdev), but the following
 *	brief explaination may help.  The interface between the kernel
 *	and the server process is based on filesystem streams.  The
 *	server gets a "control stream" when it opens the pseudo-device
 *	file with the FS_MASTER flag.  This control stream is readable
 *	when a new client process has opened the pseudo-device.  The
 *	control stream contains a short message with a new filesystem
 *	streamID for a "service stream" that the server uses to communicate
 *	with the new client.
 *	Service streams are used by the server to get request messages
 *	(that correspond to client operations) and return results.
 *
 *	Associated with each service stream is a "request buffer".  The server
 *	gets request messages from clients indirectly; the requests are
 *	placed into the request buffer, along with their data, and the server
 *	learns about new requests by reading messages from the service stream
 *	that contain 'firstByte' and 'lastByte' offsets into the request buffer.
 *	This buffered interface lets the kernel stack up many requests (writes,
 *	in particular) before a	context switch is required to the server
 *	program.  Of course, this also lets the server handle all the queued
 *	requests at one time.  When the	server has handled requests it updates
 *	the 'firstByte' pointer by making a IOC_PDEV_SET_PTRS ioctl() on
 *	the service stream.
 *
 *	There can also be a read ahead buffer associated with each service
 *	stream.  This is filled with data by the server program that is to
 *	be read by a client, and the kernel moves data from this buffer,
 *	which is again in the server's address space, to the client without
 *	having to switch out to the server process. 
 *	
 * Copyright 1987 Regents of the University of California
 * All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 * $Header: /user5/kupfer/spriteserver/1.098/include/dev/RCS/pdev.h,v 1.1 91/11/17 18:30:28 kupfer Exp $ SPRITE (Berkeley)
 */

#ifndef _PDEV
#define _PDEV

#include "proc.h"
#ifdef KERNEL
#include "fs.h"
#else
#include <kernel/fs.h>
#endif

/*
 * Pseudo-device operations
 *	PDEV_OPEN		The first request after a client open.
 *	PDEV_DUP		OBSOLETE, but might resurrect itself.
 *	PDEV_CLOSE		The last request.
 *	PDEV_READ		Read data from the pseudo-device.
 *	PDEV_WRITE		Write data to the pseudo-device.
 *	PDEV_IOCTL		Special operation on the pseudo-device.
 *	PDEV_WRITE_ASYNC	Asynchronous write.  No reply needed.
 *
 * These two only apply to pseudo-device connections to pseudo-file-systems.
 * For regular pseudo-devices the kernel takes care of attribute handling.
 *	PDEV_GET_ATTR		Get attributes given a pdev connection.
 *	PDEV_SET_ATTR		Set attributes given a pdev connection.
 */

typedef int Pdev_Op;

#define PDEV_OPEN		1
/* #define PDEV_DUP		2 */
#define PDEV_CLOSE		3
#define PDEV_READ		4
#define PDEV_WRITE		5
#define PDEV_IOCTL		6
#define PDEV_WRITE_ASYNC	7
#define PDEV_GET_ATTR		8
#define PDEV_SET_ATTR		9

/*
 * A pseudo-device server gets a 'control stream' when opening a pseudo-device.
 * The following structure describes the messages read from the control stream.
 * Control stream messages are used to notify the server that it
 * has a new private stream because a client opened the pseudo-device.
 */
typedef struct Pdev_Notify {
    unsigned int	magic;			/* == PDEV_NOTIFY_MAGIC */
    int			newStreamID;
    int			reserved;		/* Extra */
} Pdev_Notify;

#define PDEV_NOTIFY_MAGIC	0x1D4E4F52

/*
 * The first message on a private stream is a PDEV_OPEN.
 * This message identifies the client process, its host, and the host's
 * byte order to the server.
 */

typedef struct Pdev_OpenParam {
    int flags;			/* Flags from the Fs_Open call */
    Proc_PID pid;		/* Client's process ID */
    int hostID;			/* Host ID where client is from */
    int uid;			/* User ID of the client process */
    int gid;			/* Group ID of the client process */
    int byteOrder;		/* Byte order identifier */
    int reserved;		/* Extra */
} Pdev_OpenParam;

/*
 * PDEV_READ, PDEV_WRITE request parameters, see <kernel/fsIO.h>
 */

typedef Fs_IOParam Pdev_RWParam;

/*
 * PDEV_IOCTL request parameter, see <kernel/fsIO.h>
 */

typedef Fs_IOCParam Pdev_IOCParam;

/*
 * PDEV_SET_ATTR request parameters.  These are in the Pdev_Request header,
 * and the new Fs_Attributes record is passed as the data block following
 * the header.  The user and group ID are used for authentication.
 */

typedef struct {
    int		flags;		/* Which attributes to set */
    int		uid;		/* User ID */
    int		gid;		/* Group ID */
} Pdev_SetAttrParam;


/*
 * When a client does something to the pseudo-device the server gets
 * a corresponding request message.  The structure of it is defined here.
 * The control information in the request header is the same for both
 * pseudo-device and pseudo-filesystem operations.
 */

typedef struct {
    unsigned int magic;		/* PDEV_REQUEST_MAGIC or PFS_REQUEST_MAGIC */
    Pdev_Op	operation;	/* What action is requested. */
    int		messageSize;	/* The complete size of the request header
				 * plus data, plus padding for alignment */
    int		requestSize;	/* Size of data following this header */
    int		replySize;	/* Max size of the reply data expected. */
    int		dataOffset;	/* Offset of data from start of header */
} Pdev_RequestHdr;

typedef struct {
    Pdev_RequestHdr	hdr;	/* with PDEV_REQUEST_MAGIC */
    union {			/* Additional parameters to the operation. */
	Pdev_OpenParam		open;
	Pdev_RWParam		read;
	Pdev_RWParam		write;
	Pdev_IOCParam		ioctl;
	Pdev_SetAttrParam	setAttr;
    } param;
} Pdev_Request;

#define PDEV_REQUEST_MAGIC	0x7265717E

/*
 * IOC_PDEV_REPLY is used to return the following information about
 * a reply to a client's request.  The status in
 * the reply header will be the return value of the client's system call,
 * except for FS_WOULD_BLOCK and FS_LOOKUP_REDIRECT which are processed
 * by the kernel.
 */

typedef struct Pdev_Reply {
    unsigned int magic;		/* PDEV_REPLY_MAGIC */
    ReturnStatus status;	/* Return status of remote call */
    int		selectBits;	/* Return select state bits */
    int		replySize;	/* Size of the data in replyBuf, if any */
    Address	replyBuf;	/* Server space address of reply data */
    int		signal;		/* Signal to return, if non-zero */
    int		code;		/* Code to modify signal */
} Pdev_Reply;

#define PDEV_REPLY_MAGIC	0x52455057

/*
 * IOC_PDEV_SMALL_REPLY uses the following struct to return a small
 * amount of data to the client's request.
 * Up to PDEV_SMALL_DATA_LIMIT bytes can be returned.
 * 
 */
#define PDEV_SMALL_DATA_LIMIT	16

typedef struct Pdev_ReplyData {
    unsigned int magic;		/* PDEV_REPLY_DATA_MAGIC */
    ReturnStatus status;	/* Return status of remote call */
    int		selectBits;	/* Return select state bits */
    int		replySize;	/* Size of following data */
    Address	replyBuf;	/* Unused, needed for padding & compatibility */
    int		signal;		/* (non-zero) Signal to generate, if any */
    int		code;		/* Code to modify the signal */
    char	data[PDEV_SMALL_DATA_LIMIT];	/* Reply data */
} Pdev_ReplyData;

#define PDEV_REPLY_DATA_MAGIC	0x524ABC57

/*
 * I/O Controls for server streams.
 *	IOC_PDEV_READY		The server uses this to notify the kernel
 *				that the pseudo-device is ready for I/O now.
 *				The input buffer should contain an int
 *				with an or'd combination of FS_READABLE,
 *				FS_WRITABLE, or FS_EXCEPTION.
 *	IOC_PDEV_SET_BUF	These are used to tell the kernel where the
 *				request buffer and read ahead buffer (if any)
 *				are. The input buffer should contain a
 *				Pdev_SetBufArgs struct.  Note that this
 *				needs to be done after getting a notification
 *				on the control stream before any request
 *				(i.e. the client's open request) comes through.
 *				This can also be done later to change the
 *				buffer if needed.  The buffer change takes
 *				place as soon as the previous one empties.
 *				The switch is indicated by the requestAddr
 *				that you read from the service stream.
 *	IOC_PDEV_WRITE_BEHIND	Set (Unset) write-behind buffering in the
 *				request buffer.  The single input argument
 *				is a pointer to a Boolean; TRUE enables
 *				write-behind, FALSE inhibits it.  The default
 *				is no write-behind.
 *	IOC_PDEV_SET_PTRS	These are used to update the firstByte and
 *				lastByte pointers into the request and
 *				read ahead buffers.  The input buffer
 *				is a Pdev_BufPtrs structure.
 *	IOC_PDEV_REPLY		This is used to send a reply to a request.
 *				The input buffer contains a Pdev_Reply.  This
 *				includes an address (in the server's space)
 *				of a buffer containing reply data, if any.
 *	IOC_PDEV_SMALL_REPLY	This is like IOC_PDEV_REPLY, except that
 *				the reply data is embedding in the struct
 *				passed into the kernel.  The amount of data
 *				that can be returned this was is defined
 *				by PDEV_SMALL_DATA_LIMIT.
 *	IOC_PDEV_BIG_WRITES	Set (Unset) the ability of the client to
 *				write a chunk larger than will fit into
 *				the request buffer.  This is to support
 *				UDP socket semantics that prevent a client
 *				from writing more than the declared packet size.
 *				(Of course, we could do better by keeping the
 *				 existing semantics that automatically break
 *				 the write into smaller ones...)
 *				The input buffer should reference a Boolean;
 *				TRUE enables big writes (which is the default)
 *				FALSE prevents big writes.
 *	IOC_PDEV_SIGNAL_OWNER	This sends a signal to the controlling process
 *				of the pseudo-device.  If there is no owner
 *				then this is ignored.  The input buffer contains
 *				the signal number and code to send.
 *
 */

#define IOC_PDEV		(2 << 16)
#define IOC_PDEV_READY		(IOC_PDEV | 0x1)
#define IOC_PDEV_SET_BUF	(IOC_PDEV | 0x2)
#define IOC_PDEV_WRITE_BEHIND	(IOC_PDEV | 0x3)
#define IOC_PDEV_SET_PTRS	(IOC_PDEV | 0x4)
#define IOC_PDEV_REPLY		(IOC_PDEV | 0x5)
#define IOC_PDEV_BIG_WRITES	(IOC_PDEV | 0x6)
#define IOC_PDEV_SIGNAL_OWNER	(IOC_PDEV | 0x7)
#define IOC_PDEV_SMALL_REPLY	(IOC_PDEV | 0x8)

/*
 * Input structure for the IOC_PDEV_SET_BUF IOControl
 */

typedef struct Pdev_SetBufArgs {
    Address	requestBufAddr;		/* Server's address of request buffer */
    int		requestBufSize;		/* Num bytes in the request buffer */
    Address	readBufAddr;		/* NULL if no read ahead.  Non-NULL
					 * implicitly enables read-ahead. */
    int		readBufSize;		/* Num bytes in the read ahead buffer */
} Pdev_SetBufArgs;

/*
 * Input structure for IOC_PDEV_SET_PTRS IOControl.
 */

typedef struct Pdev_BufPtrs {
    int magic;			/* == PDEV_BUF_PTR_MAGIC */
    Address	requestAddr;	/* The address of the request buffer.  This is
				 * valid when reading only, and it indicates
				 * what request buffer is being used.  See
				 * IOC_PDEV_SET_BUF. */
    int requestFirstByte;	/* Byte offset of the first valid data byte
				 * in the request buffer.  If -1 buf is empty */
    int requestLastByte;	/* Byte offset of the last valid data byte. */
    int readFirstByte;		/* Byte offset of the first valid data byte
				 * in the read ahead buffer. -1 => empty */
    int readLastByte;		/* Byte offset of the last data byte. */
} Pdev_BufPtrs;

#define PDEV_BUF_PTR_MAGIC	0x3C46DF14

/*
 * Stucture for the IOC_PDEV_SIGNAL_OWNER operation.
 */
typedef struct Pdev_Signal {
    unsigned int signal;
    unsigned int code;
} Pdev_Signal;

#endif _PDEV
