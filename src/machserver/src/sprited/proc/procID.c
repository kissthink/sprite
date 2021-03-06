/*
 *  procID.c --
 *
 *	Routines to get and set the various identifiers of a process.
 *	The routines implement the system calls of the same name. 
 *	Synchronization to process table entries is done by locking the
 *	process's PCB.
 *
 * Copyright (C) 1986, 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/sprited/proc/RCS/procID.c,v 1.9 92/07/16 18:06:54 kupfer Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <bstring.h>
#include <ckalloc.h>
#include <mach.h>
#include <sprite.h>
#include <status.h>

#include <fs.h>
#include <proc.h>
#include <sig.h>
#include <spriteSrvServer.h>
#include <sync.h>
#include <vm.h>

/*
 * Define a macro to get the minimum of two values.  Note: it is *not*
 * side-effect free.  If "a" or "b" is a function call, the function will
 * be called twice.  Of course, there shouldn't be side-effects in
 * conditional expressions.
 */

#define Min(a,b) ((a) < (b) ? (a) : (b))


/*
 *----------------------------------------------------------------------
 *
 * Proc_GetIDsStub --
 *
 *	Returns the process ID, user ID and effective user ID of the current
 *	process.
 *
 * Results:
 *	Returns KERN_SUCCESS.  Fills in the "pending signals" flag.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

kern_return_t
Proc_GetIDsStub(serverPort, procIDPtr, parentIDPtr, userIDPtr,
		effUserIDPtr, sigPendingPtr)
    mach_port_t serverPort;	/* request port */
    Proc_PID 	*procIDPtr;	/* Where to return pid. */
    Proc_PID 	*parentIDPtr;	/* Where to return parent's pid */
    int 	*userIDPtr;	/* Where to return real user id. */
    int 	*effUserIDPtr;	/* Where to return effective user id. */
    boolean_t	*sigPendingPtr;	/* OUT: is there a signal pending */
{
    register Proc_ControlBlock 	*procPtr;
    Proc_PID myPID;

#ifdef lint
    serverPort = serverPort;
#endif
    procPtr = Proc_GetEffectiveProc();

    /*
     *  Copy the information to the out parameters.  Unlike native Sprite, 
     *  none of the pointers is allowed to be nil.
     */
    
    /*
     * Return the process ID this process thinks it is, not necessarily
     * the one in the PCB.
     */
    if (procPtr->genFlags & PROC_FOREIGN) {
	myPID = procPtr->peerProcessID;
    } else {
	myPID = procPtr->processID;
    }
    *procIDPtr = myPID;

    *parentIDPtr = procPtr->parentID;
    *userIDPtr = procPtr->userID;
    *effUserIDPtr = procPtr->effectiveUserID;

    *sigPendingPtr = Sig_Pending(Proc_GetCurrentProc());
    return KERN_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * Proc_SetIDs --
 *
 *	Changes the user ID or current user ID for the current process.
 *  	If an argument is USER_NIL, the corresponding value in
 *	the PCB structure is not changed.
 *
 * Results:
 *	SYS_ARG_NOACCESS - 	the arguments were not accessible.
 *	PROC_INVALID_PID -	the pid argument was illegal.
 *
 * Side effects:
 *	The user ID and/or effective user ID for a process may change.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
Proc_SetIDs(userID, effUserID)
    int 	userID;
    int 	effUserID;
{
    register	Proc_ControlBlock 	*procPtr;

    procPtr = Proc_GetEffectiveProc();

    if (userID == PROC_NO_ID) {
	userID = procPtr->userID;;
    }
    if (effUserID == PROC_NO_ID) {
	effUserID = procPtr->effectiveUserID;
    }
    if (userID != procPtr->userID && userID != procPtr->effectiveUserID &&
	procPtr->effectiveUserID != PROC_SUPER_USER_ID) {
       return(PROC_UID_MISMATCH);
    }
    if (effUserID != procPtr->userID && effUserID != procPtr->effectiveUserID &&
	procPtr->effectiveUserID != PROC_SUPER_USER_ID) {
       return(PROC_UID_MISMATCH);
    }
    procPtr->userID = userID;
    procPtr->effectiveUserID = effUserID;

#ifdef SPRITED_MIGRATION
    if (procPtr->state == PROC_MIGRATED) {
	return(Proc_MigUpdateInfo(procPtr));
    }
#endif
    return(SUCCESS);
}


/*
 *----------------------------------------------------------------------
 *
 * Proc_GetGroupIDs --
 *
 *	Returns all the group IDs of a process if the gidArrayPtr
 *	argument is not USER_NIL. Also returns the actual number of
 *	groups IDs in the process's PCB structure if trueNumGidsPtr
 *	is not USER_NIL.
 *
 *	TODO: Move me to fs.
 *
 * Results:
 *	SYS_ARG_NOACCESS - 	the arguments were not accessible.
 *	SYS_INVALID_ARG - 	the argument was was invalid.
 *	PROC_INVALID_PID -	the pid argument was illegal.
 *	The group IDs are returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
Proc_GetGroupIDs(numGIDs, gidArrayPtr, trueNumGIDsPtr)
    int		numGIDs;		/* Number of group ids in gidArrayPtr.*/
    int		*gidArrayPtr;		/* Array of group ids (user addr) */
    int		*trueNumGIDsPtr;	/* Number of group ids the process 
					 * has */
{
    register	Fs_ProcessState 	*fsPtr;
    int 				trueNumGIDs;

    fsPtr = (Proc_GetEffectiveProc())->fsPtr;

    if (numGIDs < 0) {
	return(SYS_INVALID_ARG);
    }
    trueNumGIDs = Min(numGIDs, fsPtr->numGroupIDs);
    if (trueNumGIDs > 0 && gidArrayPtr != USER_NIL) {
	if (Proc_ByteCopy(FALSE, trueNumGIDs * sizeof(int),
			  (Address) fsPtr->groupIDs,
			  (Address) gidArrayPtr) != SUCCESS) {
	    return(SYS_ARG_NOACCESS);
	}
    }

    *trueNumGIDsPtr = fsPtr->numGroupIDs;
    return(SUCCESS);
}


/*
 *----------------------------------------------------------------------
 *
 * Proc_SetGroupIDs --
 *
 *	Changes all the group IDs for a process.
 *
 * Results:
 *	SYS_ARG_NOACCESS - 	the argument was not accessible.
 *	SYS_INVALID_ARG - 	the argument was was invalid.
 *	PROC_INVALID_PID -	the pid argument was illegal.
 *
 * Side effects:
 *	The process's group IDs are changed.
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
Proc_SetGroupIDs(numGIDs, gidArrayPtr)
    int		numGIDs;	/* Number of group ids in gidArrayPtr. */
    int		*gidArrayPtr;	/* Array of group ids (user address) */
{
    register	Proc_ControlBlock	*procPtr;
    register	Fs_ProcessState		*fsPtr;
    int 				*newGidArrayPtr;
    int 				size;
    int					i;

    /*
     * See if there's anything to do before we validate the
     * other arguments.
     */

    if ((numGIDs <= 0)  ||  (numGIDs > 128) || (gidArrayPtr == USER_NIL)) {
	return(SYS_INVALID_ARG);
    }

    /*
     * Need to protect against abritrary group setting.
     */
    procPtr = Proc_GetEffectiveProc();
    if (procPtr->effectiveUserID != 0) {
	return(GEN_NO_PERMISSION);
    }

    Vm_MakeAccessible(VM_READONLY_ACCESS,
		    numGIDs * sizeof(int), (Address) gidArrayPtr,
		    &size, (Address *) &newGidArrayPtr);
    if (size != (numGIDs * sizeof(int))) {
	return(SYS_ARG_NOACCESS);
    }

    /*
     *  If the current group ID table is too small, allocate space
     *	for a larger one.
     */

    fsPtr = procPtr->fsPtr;
    if (fsPtr->numGroupIDs < numGIDs) {
	ckfree((Address) fsPtr->groupIDs);
	fsPtr->groupIDs = (int *) ckalloc(numGIDs * sizeof(int));
    }

    for (i=0; i < numGIDs; i++) {
	fsPtr->groupIDs[i] = newGidArrayPtr[i];
    }
    fsPtr->numGroupIDs = numGIDs;

    Vm_MakeUnaccessible((Address) newGidArrayPtr, size);

    return(SUCCESS);
}


/*
 *----------------------------------------------------------------------
 *
 * ProcAddToGroupList --
 *
 *	Add the given group ID to the given processes list of groups.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new group is added to the process's group if not already there.
 *
 *----------------------------------------------------------------------
 */
void
ProcAddToGroupList(procPtr, gid)
    Proc_LockedPCB	*procPtr;
    int			gid;
{
    register	Fs_ProcessState *fsPtr = procPtr->pcb.fsPtr;
    int 	*newGidArrayPtr;
    int		i;

    /*
     * See if this gid is already in the list.
     */
    for (i = 0; i < fsPtr->numGroupIDs; i++) {
	if (gid == fsPtr->groupIDs[i]) {
	    return;
	}
    }

    /*
     * Have to add the new group ID to the list.
     */
    newGidArrayPtr = (int *)ckalloc((fsPtr->numGroupIDs + 1) * sizeof(int));
    bcopy((Address)fsPtr->groupIDs, (Address)newGidArrayPtr,
	    sizeof (int) * fsPtr->numGroupIDs);
    ckfree((Address)fsPtr->groupIDs);
    fsPtr->groupIDs = newGidArrayPtr;
    fsPtr->groupIDs[fsPtr->numGroupIDs] = gid;
    fsPtr->numGroupIDs++;
}
