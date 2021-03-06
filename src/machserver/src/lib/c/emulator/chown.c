/* 
 * chown.c --
 *
 *	UNIX chown() and fchown() for the Sprite server.
 *
 * Copyright 1992 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that this copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/lib/c/emulator/RCS/chown.c,v 1.1 92/03/13 20:38:05 kupfer Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <mach.h>
#include <mach/message.h>
#include <sprite.h>
#include <compatInt.h>
#include <fs.h>
#include <spriteEmuInt.h>
#include <spriteSrv.h>
#include <string.h>


/*
 *----------------------------------------------------------------------
 *
 * chown --
 *
 *	Procedure to map from Unix chown system call to Sprite 
 *	Fs_SetAttr call.
 *
 * Results:
 *      UNIX_SUCCESS    - the call was successful.
 *      UNIX_ERROR      - the call was not successful.
 *                        The actual error code stored in errno.
 *
 * Side effects:
 *	The protection of the specified file is modified.
 *
 *----------------------------------------------------------------------
 */

int
chown(path, owner, group)
    char *path;
    int owner;
    int group;
{
    ReturnStatus status;
    Fs_Attributes attributes;	/* struct containing all file attributes.
				 * only ownership is looked at. */
    mach_msg_type_number_t pathNameLength = strlen(path) + 1;
    kern_return_t kernStatus;
    Boolean sigPending;

    attributes.uid = owner;
    attributes.gid = group;
    kernStatus = Fs_SetAttrStub(SpriteEmu_ServerPort(), path,
				pathNameLength, FS_ATTRIB_LINK, attributes,
				FS_SET_OWNER, &status, &sigPending);
    if (kernStatus != KERN_SUCCESS) {
	status = Utils_MapMachStatus(kernStatus);
    }
    if (sigPending) {
	SpriteEmu_TakeSignals();
    }
    if (status != SUCCESS) {
	errno = Compat_MapCode(status);
	return(UNIX_ERROR);
    } else {
	return(UNIX_SUCCESS);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * fchown --
 *
 *	Procedure to map from Unix fchown system call to Sprite 
 *	Fs_SetAttrID call.
 *
 * Results:
 *      UNIX_SUCCESS    - the call was successful.
 *      UNIX_ERROR      - the call was not successful.
 *                        The actual error code stored in errno.
 *
 * Side effects:
 *	The protection of the specified file is modified.
 *
 *----------------------------------------------------------------------
 */

int
fchown(fd, owner, group)
    int fd;
    int owner;
    int group;
{
    ReturnStatus status;
    Fs_Attributes attributes;	/* struct containing all file attributes,
				 * only ownship info is looked at. */
    kern_return_t kernStatus;
    Boolean sigPending;

    attributes.uid = owner;
    attributes.gid = group;
    kernStatus = Fs_SetAttrIDStub(SpriteEmu_ServerPort(), fd, attributes, 
				  FS_SET_OWNER, &status, &sigPending);
    if (kernStatus != KERN_SUCCESS) {
	status = Utils_MapMachStatus(kernStatus);
    }
    if (sigPending) {
	SpriteEmu_TakeSignals();
    }
    if (status != SUCCESS) {
	errno = Compat_MapCode(status);
	return(UNIX_ERROR);
    } else {
	return(UNIX_SUCCESS);
    }
}

