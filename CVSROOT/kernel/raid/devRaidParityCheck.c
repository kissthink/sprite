/* 
 * devRaidParityCheck.c --
 *
 *	This file implements routines for checking the parity.
 *
 * Copyright 1989 Regents of the University of California
 * All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "sync.h"
#include <string.h>
#include "stdio.h"
#include "sprite.h"
#include "fs.h"
#include "dev.h"
#include "devBlockDevice.h"
#include "devRaid.h"
#include "semaphore.h"
#include "stdlib.h"
#include "devRaidUtil.h"
#include "schedule.h"
#include "devRaidProto.h"


/*
 *----------------------------------------------------------------------
 *
 * Raid_InitiateParityCheck --
 *	
 *	Check the parity beginning at startStripe for numStripe.
 *	If numStripe is negative, all stripes will be checked.
 *	(ctrlData is used by the debug device when debugging in user mode.)
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Parity is updated.
 *
 *----------------------------------------------------------------------
 */

static void InitiateStripeParityCheck();
static void parityCheckReadDoneProc();

void
Raid_InitiateParityCheck(raidPtr, startStripe, numStripe, doneProc,clientData,ctrlData)
    Raid	*raidPtr;
    int		 startStripe;
    int		 numStripe;
    void       (*doneProc)();
    ClientData   clientData;
    int		 ctrlData;
{
    RaidReconstructionControl	*reconstructionControlPtr;
    reconstructionControlPtr =
	    Raid_MakeReconstructionControl(raidPtr, (int) NIL, (int) NIL,
		    (RaidDisk *) NIL, doneProc, clientData, ctrlData);
    reconstructionControlPtr->stripeID = startStripe;
    reconstructionControlPtr->numStripe = numStripe;
    printf("RAID:MSG:Initiating parity check.\n");
    InitiateStripeParityCheck(reconstructionControlPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * parityCheckDoneProc --
 *
 *	Callback procedure for Raid_InitiateParityCheck.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
parityCheckDoneProc(reconstructionControlPtr)
    RaidReconstructionControl	*reconstructionControlPtr;
{
    reconstructionControlPtr->doneProc(reconstructionControlPtr->clientData,
	    reconstructionControlPtr->status);
    Raid_FreeReconstructionControl(reconstructionControlPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Raid_InitiateParityCheckFailure --
 *
 *	Causes the initialization of the current stripe to fail.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Prints an error message.
 *
 *----------------------------------------------------------------------
 */

static void
Raid_InitiateParityCheckFailure(reconstructionControlPtr)
    RaidReconstructionControl	*reconstructionControlPtr;
{
    parityCheckReadDoneProc(reconstructionControlPtr, 1);
}


/*
 *----------------------------------------------------------------------
 *
 * InitiateStripeParityCheck --
 *
 *	Reconstructs the parity on a single stripe.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Locks stripe.
 *	Parity is updated.
 *
 *----------------------------------------------------------------------
 */

static void
InitiateStripeParityCheck(reconstructionControlPtr)
    RaidReconstructionControl	*reconstructionControlPtr;
{
    Raid	       *raidPtr       = reconstructionControlPtr->raidPtr;


    int	       		ctrlData      = reconstructionControlPtr->ctrlData;
    RaidRequestControl *reqControlPtr = reconstructionControlPtr->reqControlPtr;
    char	       *readBuf       = reconstructionControlPtr->readBuf;
    char	       *parityBuf     = reconstructionControlPtr->parityBuf;

    int		        stripeID      = reconstructionControlPtr->stripeID;
    int		        numStripe     = reconstructionControlPtr->numStripe;
    unsigned	        firstSector;
    unsigned	        nthSector;

    firstSector = StripeIDToSector(raidPtr, stripeID);
    nthSector   = NthSectorOfStripe(raidPtr, firstSector);
    if (stripeID >= raidPtr->numStripe || stripeID < 0 || numStripe == 0) {
	parityCheckDoneProc(reconstructionControlPtr);
	return;
    }
    Raid_SLockStripe(raidPtr, stripeID);
    reqControlPtr->numReq = reqControlPtr->numFailed = 0;
    AddRaidDataRequests(reqControlPtr, raidPtr, FS_READ,
	    firstSector, nthSector, readBuf, ctrlData);
    AddRaidParityRequest(reqControlPtr, raidPtr, FS_READ,
	    (unsigned) StripeIDToSector(raidPtr, stripeID),
	    parityBuf, ctrlData);
    if (reqControlPtr->numFailed == 0) {
	Raid_InitiateIORequests(reqControlPtr,
		parityCheckReadDoneProc,
		(ClientData) reconstructionControlPtr);
    } else {
	Raid_InitiateParityCheckFailure(reconstructionControlPtr);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * parityCheckReadDoneProc --
 *
 *	Callback procedure for InitiateStripeParityCheck.
 *	Called after the data and parity on a stripe is read.
 *	Checks the parity.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
parityCheckReadDoneProc(reconstructionControlPtr, numFailed)
    RaidReconstructionControl	*reconstructionControlPtr;
    int			 	 numFailed;
{
    Raid	       *raidPtr       = reconstructionControlPtr->raidPtr;
    char	       *parityBuf     = reconstructionControlPtr->parityBuf;
    RaidRequestControl *reqControlPtr = reconstructionControlPtr->reqControlPtr;
    int		        stripeID      = reconstructionControlPtr->stripeID;

    if (numFailed > 0) {
	Raid_ReportParityCheckFailure(reconstructionControlPtr->stripeID);
	reconstructionControlPtr->status = FAILURE;
    } else {
	char	       *xorBuf = malloc((unsigned)raidPtr->bytesPerStripeUnit);
	bzero(xorBuf, raidPtr->bytesPerStripeUnit);
	XorRaidRequests(reqControlPtr, raidPtr, xorBuf);
#ifndef NODATA
	bzero(parityBuf, raidPtr->bytesPerStripeUnit);
	if (bcmp(parityBuf, xorBuf, raidPtr->bytesPerStripeUnit) != 0) {
	    Raid_ReportParityCheckFailure(stripeID);
	    reconstructionControlPtr->status = FAILURE;
	}
	free(xorBuf);
#endif
    }
    if (reconstructionControlPtr->stripeID % 100 == 0) {
	printf("RAID:MSG:%d", reconstructionControlPtr->stripeID);
    }
    Raid_SUnlockStripe(raidPtr, reconstructionControlPtr->stripeID);
    reconstructionControlPtr->stripeID++;
    reconstructionControlPtr->numStripe--;
    InitiateStripeParityCheck(reconstructionControlPtr);
}
