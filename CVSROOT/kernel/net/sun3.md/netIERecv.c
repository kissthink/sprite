/* netIERecv.c -
 *
 * Routines to manage the receive unit of the Intel ethernet chip.
 *
 * Copyright 1985, 1988 Regents of the University of California
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
#include "netIEInt.h"
#include "net.h"
#include "netInt.h"
#include "sys.h"
#include "list.h"


/*
 *----------------------------------------------------------------------
 *
 * NetIERecvUnitInit --
 *
 *	Initialize the receive buffer lists for the receive unit and start
 *	it going.
 *
 *	NOTE: One more buffer descriptor is allocated than frame descriptor
 *	      because Sun claims that this gets rid of a microcode bug.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The receive frame descriptor and buffer lists are initialized.
 *
 *----------------------------------------------------------------------
 */

void
NetIERecvUnitInit()
{
    int i;
    register	NetIERecvBufDesc   *recvBufDescPtr;
    register	NetIERecvFrameDesc *recvFrDescPtr;
    register	NetIESCB 	   *scbPtr;
    int				   bufferSize;

    bufferSize = NET_IE_RECV_BUFFER_SIZE - sizeof(Net_EtherHdr);

    /*
     * Allocate the receive buffer descriptors.  
     */

    for (i = 0; i < NET_IE_NUM_RECV_BUFFERS; i++) {
	recvBufDescPtr = (NetIERecvBufDesc *) NetIEMemAlloc();
	if (recvBufDescPtr == (NetIERecvBufDesc *) NIL) {
	    panic("No memory for a receive buffer descriptor pointer\n");
	}

	*(short *)recvBufDescPtr = 0;	/* Clear out the status word */

	if (i == 0) {
	    netIEState.recvBufDscHeadPtr = recvBufDescPtr;
	    netIEState.recvBufDscTailPtr = recvBufDescPtr;
	} else {
	    netIEState.recvBufDscTailPtr->nextRBD = 
			NetIEOffsetFromSUNAddr((int) recvBufDescPtr);
	    netIEState.recvBufDscTailPtr->realNextRBD = recvBufDescPtr;
	    netIEState.recvBufDscTailPtr = recvBufDescPtr;
	}

	/*
	 * Point the header to its buffer.  It is pointed to the buffer plus
	 * the size of the ethernet header so that when we receive the 
	 * packet we can fill in the ethernet header.
	 */

	recvBufDescPtr->bufAddr = 
			NetIEAddrFromSUNAddr((int) (netIERecvBuffers[i] + sizeof(Net_EtherHdr)));
	recvBufDescPtr->realBufAddr = netIERecvBuffers[i];
	recvBufDescPtr->bufSizeHigh = bufferSize >> 8;
	recvBufDescPtr->bufSizeLow = bufferSize & 0xff;
	recvBufDescPtr->endOfList = 0;
    }

    /*
     * Link the last element to the first to make it circular and mark the last
     * element as the end of the list.
     */

    recvBufDescPtr->nextRBD = 
		NetIEOffsetFromSUNAddr((int) netIEState.recvBufDscHeadPtr);
    recvBufDescPtr->realNextRBD = netIEState.recvBufDscHeadPtr;
    recvBufDescPtr->endOfList = 1;

    /*
     * Now allocate the receive frame headers.
     */

    for (i = 0; i < NET_IE_NUM_RECV_BUFFERS - 1; i++) {
	recvFrDescPtr = (NetIERecvFrameDesc *) NetIEMemAlloc();
	if (recvFrDescPtr == (NetIERecvFrameDesc *) NIL) {
	    panic("No memory for a receive frame descriptor pointer\n");
	}

	*(short *)recvFrDescPtr = 0;	/* Clear out the status word */

	recvFrDescPtr->endOfList = 0;
	recvFrDescPtr->suspend = 0;

	if (i == 0) {
	    netIEState.recvFrDscHeadPtr = recvFrDescPtr;
	    netIEState.recvFrDscTailPtr = recvFrDescPtr;

	    /*
	     * The first receive frame descriptor points to the list of buffer
	     * descriptors.
	     */

	    recvFrDescPtr->recvBufferDesc = 
		    NetIEOffsetFromSUNAddr((int) netIEState.recvBufDscHeadPtr);

	} else {
	    recvFrDescPtr->recvBufferDesc = NET_IE_NULL_RECV_BUFF_DESC;
	    netIEState.recvFrDscTailPtr->nextRFD = 
			NetIEOffsetFromSUNAddr((int) recvFrDescPtr);
	    netIEState.recvFrDscTailPtr->realNextRFD = recvFrDescPtr;
	    netIEState.recvFrDscTailPtr = recvFrDescPtr;
	}
    }

    /*
     * Link the last element to the first to make it circular.
     */

    recvFrDescPtr->nextRFD = 
		    NetIEOffsetFromSUNAddr((int) netIEState.recvFrDscHeadPtr);
    recvFrDescPtr->realNextRFD = netIEState.recvFrDscHeadPtr;

    recvFrDescPtr->endOfList = 1;

    scbPtr = netIEState.scbPtr;

    /*
     * Now start up the receive unit.  To do this we first make sure that
     * it is idle.  Then we start it up.
     */

    if (scbPtr->statusWord.recvUnitStatus != NET_IE_RUS_IDLE) {
	printf("Intel: The receive unit is not idle!!!\n");

	scbPtr->cmdWord.recvUnitCmd = NET_IE_RUC_ABORT;

	NET_IE_CHANNEL_ATTENTION;
	NetIECheckSCBCmdAccept(scbPtr);
    }

    scbPtr->recvFrameAreaOffset = 
		    NetIEOffsetFromSUNAddr((int) netIEState.recvFrDscHeadPtr);
    scbPtr->cmdWord.recvUnitCmd = NET_IE_RUC_START;

    NET_IE_CHANNEL_ATTENTION;
    NetIECheckSCBCmdAccept(scbPtr);

    NET_IE_DELAY(scbPtr->statusWord.recvUnitStatus != NET_IE_RUS_READY);

    if (scbPtr->statusWord.recvUnitStatus != NET_IE_RUS_READY) {
	printf("Intel: Receive unit never became ready.\n");
    }
}


/*
 *----------------------------------------------------------------------
 *
 * NetIERecvProcess --
 *
 *	Process a newly received packet.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
NetIERecvProcess(dropPackets)
    Boolean	dropPackets;	/* Drop all packets. */
{
    register	NetIERecvBufDesc	*recvBufDescPtr;
    register	NetIERecvFrameDesc	*recvFrDescPtr;
    register	NetIEState		*netIEStatePtr;
    register	Net_EtherHdr		*etherHdrPtr;
    NetIERecvFrameDesc			*newRecvFrDescPtr;
    int					size;
    int					num;

    netIEStatePtr = &netIEState;

    recvFrDescPtr = netIEStatePtr->recvFrDscHeadPtr;

    /*
     * If not initialized then forget the interrupt.
     */

    if (recvFrDescPtr == (NetIERecvFrameDesc *) NIL) {
	return;
    }

    /*
     * Loop as long as there are packets to process.
     */

    while (recvFrDescPtr->done) {

	net_EtherStats.packetsRecvd++;

	/*
	 * If this packet has a buffer associated with it then process it.
	 */

	if ((unsigned short) recvFrDescPtr->recvBufferDesc != 
			NET_IE_NULL_RECV_BUFF_DESC) {

	    recvBufDescPtr = netIEStatePtr->recvBufDscHeadPtr;

	    size = recvBufDescPtr->countLow +
	           (recvBufDescPtr->countHigh << 8) + sizeof(Net_EtherHdr);

	    /*
	     * Put the ethernet header into the packet.
	     */

	    etherHdrPtr = (Net_EtherHdr *) recvBufDescPtr->realBufAddr;
	    etherHdrPtr->source = recvFrDescPtr->srcAddr;
	    etherHdrPtr->destination = recvFrDescPtr->destAddr;
	    etherHdrPtr->type = recvFrDescPtr->type;

	    /*
	     * Call higher level protocol to process the packet.
	     */
	    if (!dropPackets) {
		Net_Input((Address)etherHdrPtr, size);
	    }

	    /*
	     * Make the element that was just processed the last element in the
	     * list.  Since this is circular list, no relinking has to be done.
	     */

	    *(short *) recvBufDescPtr = 0;	/* Clear out the status word. */
	    recvBufDescPtr->endOfList = 1;
	    netIEStatePtr->recvBufDscTailPtr->endOfList = 0; 
	    netIEStatePtr->recvBufDscTailPtr = recvBufDescPtr;
	    netIEStatePtr->recvBufDscHeadPtr = recvBufDescPtr->realNextRBD;
	}

	/*
	 * Make the element that was just processed the last element in the
	 * list.  Since this is circular list, no relinking has to be done.
	 */

	newRecvFrDescPtr = recvFrDescPtr->realNextRFD;
	recvFrDescPtr->recvBufferDesc = NET_IE_NULL_RECV_BUFF_DESC;
	recvFrDescPtr->endOfList = 1;
	*(short *) recvFrDescPtr = 0;
	netIEStatePtr->recvFrDscTailPtr->endOfList = 0;
	netIEStatePtr->recvFrDscTailPtr = recvFrDescPtr;

	netIEStatePtr->recvFrDscHeadPtr = newRecvFrDescPtr;
	recvFrDescPtr = newRecvFrDescPtr;
    }

    /*
     * Record statistics about packets.
     */

    if (netIEStatePtr->scbPtr->crcErrors != 0) {
	num = netIEStatePtr->scbPtr->crcErrors;
	netIEStatePtr->scbPtr->crcErrors = 0;
	net_EtherStats.crcErrors += NetIEShortSwap(num);
    }

    if (netIEStatePtr->scbPtr->alignErrors != 0) {
	num = netIEStatePtr->scbPtr->alignErrors;
	netIEStatePtr->scbPtr->alignErrors = 0;
	net_EtherStats.frameErrors += NetIEShortSwap(num);
    }

    if (netIEStatePtr->scbPtr->resourceErrors != 0) {
	num = netIEStatePtr->scbPtr->resourceErrors;
	netIEStatePtr->scbPtr->resourceErrors = 0;
	net_EtherStats.recvPacketsDropped += NetIEShortSwap(num);
    }

    if (netIEStatePtr->scbPtr->overrunErrors != 0) {
	num = netIEStatePtr->scbPtr->overrunErrors;
	netIEStatePtr->scbPtr->overrunErrors = 0;
	net_EtherStats.overrunErrors += NetIEShortSwap(num);
    }

    /*
     * See if the receive unit is ready.  If it is, then return.
     */

    if (netIEStatePtr->scbPtr->statusWord.recvUnitStatus == NET_IE_RUS_READY) {
	return;
    }

    /*
     * Otherwise reinitialize the receive unit.  To do this set the head
     * receive frame pointer to point to the head of the list of buffer
     * headers and give the reinit command to the chip.
     */

printf("Reinit recv unit\n");

    netIEStatePtr->recvFrDscHeadPtr->recvBufferDesc = 
		NetIEOffsetFromSUNAddr((int) netIEStatePtr->recvBufDscHeadPtr);
    netIEStatePtr->scbPtr->recvFrameAreaOffset =
		NetIEOffsetFromSUNAddr((int) netIEStatePtr->recvFrDscHeadPtr);
    NET_IE_CHECK_SCB_CMD_ACCEPT(netIEStatePtr->scbPtr);
    netIEStatePtr->scbPtr->cmdWord.recvUnitCmd = NET_IE_RUC_START;
    NET_IE_CHANNEL_ATTENTION;
}
