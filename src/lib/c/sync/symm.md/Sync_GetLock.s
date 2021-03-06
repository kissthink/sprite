/*
 * syncAsm.s --
 *
 *	Source code for the Sync_GetLock library procedure.
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

    .data
    .asciz "$Header: /sprite/src/lib/c/sync/symm.md/RCS/Sync_GetLock.s,v 1.2 90/10/31 17:44:23 kupfer Exp $ SPRITE (Berkeley)"
    .align 2
    .text

/* $Log:	Sync_GetLock.s,v $
 * Revision 1.2  90/10/31  17:44:23  kupfer
 * Can't have comments in comments.
 * 
 * Revision 1.1  90/10/31  17:41:10  kupfer
 * Initial revision
 * 
 * Revision 1.2  90/03/05  14:41:32  rbk
 * Add call to Sync_SlowLock() if can't get it right away.
 * Cleaned up a bit (use # in line comments), and loose save/restore of
 * scratch registers.
 * 
 * Revision 1.1  90/03/05  10:13:44  rbk
 * Initial revision
 * 
 * Based on Sync_GetLock.s,v 1.1 88/06/19 14:34:17 ouster
 */

#include "kernel/machAsmDefs.h"


/*
 * ----------------------------------------------------------------------------
 *
 * Sync_GetLock --
 *
 *	Acquire a lock.  Other processes trying to acquire the lock
 *	will block until this lock is released.
 *
 *      A critical section of code is protected by a lock.  To safely
 *      execute the code, the caller must first call Sync_GetLock to
 *      acquire the lock on the critical section.  At the end of the
 *      critical section the caller has to call Sync_Unlock to release
 *      the lock and allow other processes to execute in the critical
 *      section.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      The lock is set.  Other processes will be blocked if they try
 *      to lock the same lock.  A blocked process will try to get the
 *      lock after this process unlocks the lock with Sync_Unlock.
 *
 * C equivalent:
 *
 *	void
 *	Sync_GetLock(lockPtr)
 *	   Sync_Lock *lockPtr;
 *	{
 *	    if (Sun_TestAndSet(&(lockPtr->inUse)) != 0) {
 *		Sync_SlowLock(lockPtr); 
 *	    }
 *	}
 *
 *----------------------------------------------------------------------
 * typedef struct Sync_UserLock {
 *    Boolean inUse;              // 1 while the lock is busy 
 *    Boolean waiting;            // 1 if someone wants the lock
 * } Sync_UserLock;
 *
 */

ENTRY(Sync_GetLock)
	movl	SPARG0, %eax		# address of lock
	movl	$1, %edx		# 1 == locked
	xchgl   %edx, (%eax)            # try for lock
        cmpl    $0, %edx	        # got it?
        jne     1f                      # nope -- do it the hard way.
	RETURN				# got it!
	/*
	 * Didn't get the lock on first attempt.  Call kernel.
	 */
1:	pushl	%eax			# lockPtr
	CALL	_Sync_SlowLock		# call kernel to get lock
	addl	$4, %esp		# clear stack
	RETURN				# done
