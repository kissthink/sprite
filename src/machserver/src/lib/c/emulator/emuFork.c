/* 
 * emuFork.c --
 *
 *	Emulator support for fork operation.
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
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/lib/c/emulator/RCS/emuFork.c,v 1.2 92/03/12 19:23:12 kupfer Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <compatInt.h>
#include <errno.h>
#include <mach.h>
#include <proc.h>
#include <sprite.h>
#include <spriteEmuInt.h>
#include <spriteEmuMach.h>
#include <spriteSrv.h>
#include <sys/types.h>


/*
 *----------------------------------------------------------------------
 *
 * fork --
 *
 *	Spawn a new process.
 *
 * Results:
 *	If a process is created, returns the process ID to the parent and 0 
 *	to the child.  If there was an error, returns UNIX_ERROR and sets
 *	errno to the UNIX error value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

pid_t
fork()
{
    long stateArray[SPRITEEMUMACH_STATE_ARRAY_WORDS];
    kern_return_t kernStatus;
    ReturnStatus status;
    Proc_PID pid;
    Boolean sigPending;

    /* 
     * This code relies on some assembly language magic.  The child process 
     * will get started at SpriteEmuMach_ChildInit, with a well-known 
     * register pointing at whatever state SpriteEmuMach_SaveState set up.
     * It will do some initialization, fix up the stack and any other
     * necessary registers, and return as though from 
     * SpriteEmuMach_SaveState().  The parent sees a non-zero return from 
     * SpriteEmuMach_SaveState; the child sees a return of 0.
     */
    if (SpriteEmuMach_SaveState(stateArray) == 0) {
	return 0;
    }
    kernStatus = Proc_ForkStub(SpriteEmu_ServerPort(),
			       (vm_address_t)SpriteEmuMach_ChildInit,
			       (vm_address_t)stateArray, &status, &pid,
			       &sigPending);
    if (kernStatus != KERN_SUCCESS) {
	status = Utils_MapMachStatus(kernStatus);
    }
    if (sigPending) {
	SpriteEmu_TakeSignals();
    }
    if (status == SUCCESS) {
	return (pid_t)pid;
    } else {
	errno = Compat_MapCode(status);
	return UNIX_ERROR;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * vfork --
 *
 *	Virtual fork.  This isn't the real vfork, it's just a hack to get 
 *	various programs to compile.  Programs that rely on vfork semantics 
 *	(i.e., the child knows it has the parent's resources) will lose.
 *
 * Results:
 *	Same as fork.
 *
 * Side effects:
 *	Same as fork.
 *
 *----------------------------------------------------------------------
 */

pid_t
vfork()
{
    return fork();
}
