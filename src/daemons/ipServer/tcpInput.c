/* 
 * tcpInput.c --
 *
 *	Routines to handle TCP packets. The packet is examined for
 *	proper format before the data are saved in a socket's recv buffer.
 *
 *	The algorithms in this file are based on the functional specification
 *	of the Transmission Control Protocol in RFC793 (Sept. 1981).
 *
 *	Security and precedence checks are not performed on the packet.
 *
 * 	Based on the following 4.3BSD file:
 *	"@(#)tcp_input.c 7.18 (Berkeley) 5/14/88"
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
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/daemons/ipServer/RCS/tcpInput.c,v 1.14 91/10/25 10:47:56 mottsmth Exp $ SPRITE (Berkeley)";
#endif not lint


#include "sprite.h"
#include "netInet.h"
#include "ipServer.h"
#include "stat.h"
#include "ip.h"
#include "tcp.h"
#include "tcpInt.h"
#include "socket.h"
#include "route.h"

#include "list.h"
#include "fs.h"

/*
 * Global variables for
 */
int	tcpISS = 0;
int	tcpReXmtThresh = 3;

/*
 * Information about data segments on the reassembly queue of a socket's TCB.
 */
typedef struct {
    List_Links		links;		/* Used to chain elements together. */
    Address		data;		/* Ptr to the segment data. */
    Address		base;		/* Ptr to the segment base. This address
					 * is used with free. */
    int			len;		/* Length of the data. */
    unsigned short	flags;		/* TCP header flags of the segment. */
    TCPSeqNum		seqNum;		/* TCP sequence number of the segment.*/
} ReassElement;


static void	Input();
static void	ProcessOptions();
extern int	TCPCalcMaxSegSize();
static void	CalcFreeSpace();
static unsigned short	Reassemble();


/*
 *----------------------------------------------------------------------
 *
 * TCP_MemBin --
 *
 *	Bucketize various structures that are dynamically allocated by TCP.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Calls Mem_Bin.
 *
 *----------------------------------------------------------------------
 */

void
TCP_MemBin()
{
    Mem_Bin(sizeof(ReassElement));
    Mem_Bin(sizeof(TCPControlBlock));
    Mem_Bin(sizeof(TCPConnectInfo));
    Mem_Bin(sizeof(Net_TCPHeader));
    Mem_Bin(TCP_BUF_SIZE);
}

/*
 *----------------------------------------------------------------------
 *
 * TCP_Init --
 *
 *	Initialize TCP data structures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A routine is set up to handle TCP packets. The initial send
 *	sequence number is determined.
 *
 *----------------------------------------------------------------------
 */

void
TCP_Init(seconds)
    int	seconds;	/* Used to set the initial send sequence #. */
{
    if (ips_Debug) {
	(void) (void) fprintf(stderr, "TCP: sizeof(tcb) = %d\n", 
				sizeof(TCPControlBlock));
    }
    tcpISS = seconds;
    IP_SetProtocolHandler(NET_IP_PROTOCOL_TCP, Input);
    TCPTimerInit();
}


/*
 *----------------------------------------------------------------------
 *
 * Input --
 *
 *	This routine accepts IP datagrams and processes data for
 *	this protocol.
 *
 *	TCP input routine, follows pages 65-76 of the
 *	protocol specification dated September, 1981 very closely.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Many.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static void
Input(netID, packetPtr)
    Rte_NetID	netID;		/* Which network the packet came from. */
    IPS_Packet	*packetPtr;	/* Packet descriptor. */
{
    register Net_TCPHeader	*tcpHdrPtr;
    register Net_IPHeader	*ipHdrPtr;
    register TCPControlBlock	*tcbPtr;
    register unsigned short	headerFlags;
    int				dataLen;
    int				offset;
    Address			data;
    Net_TCPHeader		saveHeader;
    Sock_InfoPtr		sockPtr;
    TCPState 			oldState;
    int 			toDrop;
    int 			acked;
    Boolean			ourFinIsAcked;
    int 			needOutput = FALSE;
    int				iss = 0;
    Boolean			freePacket = TRUE;

    stats.tcp.recv.total++;

    ipHdrPtr  = packetPtr->ipPtr;
    tcpHdrPtr =(Net_TCPHeader *) ((Address) ipHdrPtr + (ipHdrPtr->headerLen*4));
    dataLen   = ipHdrPtr->totalLen - (ipHdrPtr->headerLen*4);
    data      = (Address) tcpHdrPtr;

    if (dataLen < sizeof(Net_TCPHeader)) {
	stats.tcp.recv.shortLen++;
	free(packetPtr->base);
	return;
    }

    /*
     * Make sure the shecksum is valid.
     */
    {
	Net_IPPseudoHdr		pseudoHdr;
	unsigned short		sum;

	/*
	 * The checksum is computed for the IP "pseudo-header" and
	 * the TCP header and data. When the TCP checksum was calculated,
	 * the checksum field in the header was set to zero. When we 
	 * recalculate the value, we don't zero the field so the computed 
	 * value should be zero if the packet didn't get garbled.
	 */
	pseudoHdr.source	= ipHdrPtr->source;
	pseudoHdr.dest		= ipHdrPtr->dest;
	pseudoHdr.zero		= 0;
	pseudoHdr.protocol	= ipHdrPtr->protocol;
	pseudoHdr.len		= Net_HostToNetShort((unsigned short)dataLen);
	sum = Net_InetChecksum2(dataLen, (Address) tcpHdrPtr, &pseudoHdr);
	if (sum != 0) {
	    if (ips_Debug) {
		(void) fprintf(stderr, "TCP: checksum != 0 (%x)\n", sum);
	    }

	    stats.tcp.recv.badChecksum++;
	    free(packetPtr->base);
	    return;
	}
    }

    /*
     * Make sure the data offset is valid.
     */

    offset = tcpHdrPtr->dataOffset * 4;
    if ((offset < sizeof(Net_TCPHeader)) || (offset > dataLen)) {
	stats.tcp.recv.badOffset++;
	if (ips_Debug) {
	    (void) fprintf(stderr, "TCP Input: bad offset (%d)\n", offset);
	}
	free(packetPtr->base);
	return;
    }
    dataLen -= offset;
    data    += offset;

    /*
     * The packetPtr must contain current values of the data length and
     * buffer address. Whenever packetPtr is used below, it will be
     * updated with the latest values of dataLen and data.
     */
    packetPtr->dataLen = dataLen;
    packetPtr->data    = data;


    /*
     * Convert TCP protocol specific fields to host format.
     */
#ifdef notdef
    tcpHdrPtr->srcPort	 = Net_NetToHostShort(tcpHdrPtr->srcPort);
    tcpHdrPtr->destPort	 = Net_NetToHostShort(tcpHdrPtr->destPort);
#endif
    tcpHdrPtr->window	 = Net_NetToHostShort(tcpHdrPtr->window);
    tcpHdrPtr->seqNum	 = Net_NetToHostInt(tcpHdrPtr->seqNum);
    tcpHdrPtr->ackNum	 = Net_NetToHostInt(tcpHdrPtr->ackNum);
    tcpHdrPtr->urgentOffset = Net_NetToHostShort(tcpHdrPtr->urgentOffset);
    headerFlags = tcpHdrPtr->flags;

    if (ips_Debug) {
	(void) fprintf(stderr, 
			"TCP Input: %d bytes <%x,%d> <-- <%x,%d,#%d>\n\t",
			dataLen,
			Net_NetToHostInt(ipHdrPtr->dest), 
			Net_NetToHostShort(tcpHdrPtr->destPort),
			Net_NetToHostInt(ipHdrPtr->source),
			Net_NetToHostShort(tcpHdrPtr->srcPort),
			ipHdrPtr->ident);
	TCPPrintHdrFlags(stderr, headerFlags);
	(void) fprintf(stderr, "  seq=%d,  ack=%d\n",
			tcpHdrPtr->seqNum,tcpHdrPtr->ackNum);
    }

    /*
     * Locate socket/tcb info for the segment.
     */

findSock:

    tcbPtr = (TCPControlBlock *) NULL;

    sockPtr = (Sock_InfoPtr) Sock_Match(TCP_PROTO_INDEX, 
			ipHdrPtr->dest, tcpHdrPtr->destPort, 
			ipHdrPtr->source, tcpHdrPtr->srcPort, 
			TRUE);

    /*
     * If the state is CLOSED (i.e., TCB does not exist) then
     * all data in the incoming segment is discarded. If the TCB exists
     * but is in the CLOSED state, then it's embryonic so it should
     * do a listen or a connect soon.
     */
    if (sockPtr == (Sock_InfoPtr) NULL) {
	goto dropWithReset;
    }

    tcbPtr = TCPSockToTCB(sockPtr);
    if (tcbPtr == (TCPControlBlock *) NULL) {
	goto dropWithReset;
    }
    if (tcbPtr->state == CLOSED) {
	goto drop;
    }

    if (Sock_IsOptionSet(sockPtr, NET_OPT_DEBUG)) {
	oldState = tcbPtr->state;
	saveHeader = *tcpHdrPtr;
    }



    /*
     * p. 65
     *
     * If the socket is in the LISTEN state, then ignore the segment if the
     * RESET bit is set.  If the segment contains an ACK, then it is bad --
     * send a RESET to the peer.  If the segment does not contain a SYN, then 
     * it is not interesting and it is dropped.  Don't bother responding if 
     * the destination was a broadcast.  Otherwise initialize 
     * tcbPtr->recv.next, and tcbPtr->recv.initial, select a value for
     * tcbPtr->send.initial, and send a segment with:
     *
     *     <SEQ=ISS><ACK=RECV.NEXT><CTL=SYN,ACK> 
     *
     * Also initialize tcbPtr->send.next to tcbPtr->send.initial+1 and
     * tcbPtr->send.unAck to tcbPtr->send.initial.  Fill in the remote
     * peer address fields if not previously specified.  Enter the
     * SYN_RECEIVED state, and process any other fields of this segment 
     * in this state.
     */

    if (tcbPtr->state == LISTEN) {

	if (headerFlags & NET_TCP_RST_FLAG) {
	    goto drop;
	}
	if (headerFlags & NET_TCP_ACK_FLAG) {
	    goto dropWithReset;
	}
	if ((headerFlags & NET_TCP_SYN_FLAG) == 0) {
	    goto drop;
	}
	if (Rte_IsBroadcastAddr(ipHdrPtr->dest)) {
	    goto drop;
	}

	/*
	 * Try to create a new socket and TCB. If there's and error then
	 * drop the segment.
	 */
	sockPtr = TCPCloneConnection(sockPtr, 
		    ipHdrPtr->dest, 
		    tcpHdrPtr->destPort, ipHdrPtr->source, tcpHdrPtr->srcPort);
	if (sockPtr == (Sock_InfoPtr) NULL) {
	    goto drop;
	}
	tcbPtr = TCPSockToTCB(sockPtr);


	TCPMakeTemplateHdr(sockPtr, tcbPtr);
	if (offset > sizeof(Net_TCPHeader)) {
	    ProcessOptions(offset, tcpHdrPtr, tcbPtr);
	}

	/*
	 * Select the initial send and receive sequence numbers and initialize
	 * the other fields.
	 */

	if (iss != 0) {
	    tcbPtr->send.initial = iss;
	} else {
	    tcbPtr->send.initial = tcpISS;
	}
	tcpISS += TCP_INIT_SEND_SEQ_INCR/2;
	tcbPtr->recv.initial = tcpHdrPtr->seqNum;
	TCP_SEND_SEQ_INIT(tcbPtr);
	TCP_RECV_SEQ_INIT(tcbPtr);

	tcbPtr->flags |= TCP_ACK_NOW;
	tcbPtr->state = SYN_RECEIVED;
	tcbPtr->idle = 0;
	tcbPtr->timer[TCP_TIMER_KEEP_ALIVE] = TCP_KEEP_TIME_INIT;
	CalcFreeSpace(sockPtr, tcbPtr);
	stats.tcp.accepts++;
	goto trimThenUpdateWindow;

    } 



    if (offset > sizeof(Net_TCPHeader)) {
	ProcessOptions(offset, tcpHdrPtr, tcbPtr);
    }

    /*
     * A segment was received on an active connection.  Reset the idle time 
     * and the keep-alive timer.
     */
    tcbPtr->idle = 0;
    tcbPtr->timer[TCP_TIMER_KEEP_ALIVE] = tcpKeepIdle;
    CalcFreeSpace(sockPtr, tcbPtr);
    


    if (tcbPtr->state == SYN_SENT) {

	/*
	 * p. 66
	 *
	 * If the TCB state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RESET, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment.
	 *	1) Initialize tcbPtr->recv.next and tcbPtr->recv.initial
	 *	2) If seg contains ack then advance tcbPtr->send.unAck.
	 *	3) If SYN has been acked, change to ESTABLISHED state, else
	 *	change to the SYN_RCVD state.
	 *	4) Arrange for segment to be acked (eventually)
	 *	5) Continue processing the rest of the data/controls, 
	 *	   beginning with the URG bit.
	 */

	if ((headerFlags & NET_TCP_ACK_FLAG) &&
	    (TCP_SEQ_LE(tcpHdrPtr->ackNum, tcbPtr->send.initial) ||
	     TCP_SEQ_GT(tcpHdrPtr->ackNum, tcbPtr->send.maxSent))) {
	    goto dropWithReset;
	}

	if (headerFlags & NET_TCP_RST_FLAG) {
	    if (headerFlags & NET_TCP_ACK_FLAG) {
		TCPDropConnection(sockPtr, tcbPtr, 
					(ReturnStatus)NET_CONNECT_REFUSED);
		tcbPtr = NULL;
	    }
	    goto drop;
	}

	if ((headerFlags & NET_TCP_SYN_FLAG) == 0) {
	    goto drop;
	}

	if (headerFlags & NET_TCP_ACK_FLAG) {
	    tcbPtr->send.unAck = tcpHdrPtr->ackNum;
	    if (TCP_SEQ_LT(tcbPtr->send.next, tcbPtr->send.unAck)) {
		tcbPtr->send.next = tcbPtr->send.unAck;
	    }
	}

	tcbPtr->timer[TCP_TIMER_REXMT] = 0;
	tcbPtr->recv.initial = tcpHdrPtr->seqNum;
	TCP_RECV_SEQ_INIT(tcbPtr);
	tcbPtr->flags |= TCP_ACK_NOW;

	if ((headerFlags & NET_TCP_ACK_FLAG) && 
	    TCP_SEQ_GT(tcbPtr->send.unAck, tcbPtr->send.initial)) {

	    stats.tcp.connects++;
	    Sock_Connected(sockPtr);
	    tcbPtr->state = ESTABLISHED;
	    tcbPtr->maxSegSize = MIN(tcbPtr->maxSegSize, 
					    TCPCalcMaxSegSize(tcbPtr));
	    packetPtr->data = data;
	    packetPtr->dataLen = dataLen;
	    (void) Reassemble(sockPtr, tcbPtr, (Net_TCPHeader *)NULL,
				packetPtr);
	    /*
	     * If we didn't have to retransmit the SYN, use its round-trip
	     * time as our initial smooth rrt and rtt var.
	     */
	    if (tcbPtr->rtt != 0) {
		tcbPtr->srtt = tcbPtr->rtt << 3;
		tcbPtr->rttvar = tcbPtr->rtt << 1;
		TCP_TIMER_RANGESET(tcbPtr->rxtcur, 
			((tcbPtr->srtt >> 2) + tcbPtr->rttvar) >> 1,
			TCP_MIN_REXMT_TIME, TCP_MAX_REXMT_TIME);
		tcbPtr->rtt = 0;
	    }
	} else {
	    tcbPtr->state = SYN_RECEIVED;
	}

trimThenUpdateWindow:

	/*
	 * Advance tcpHdrPtr->seqNum to correspond to first data byte.
	 * If there's data, remove the excess to stay within window, 
	 * dropping the FIN if necessary.
	 */
	tcpHdrPtr->seqNum++;
	if (dataLen > tcbPtr->recv.window) {
	    toDrop = dataLen - tcbPtr->recv.window;
	    dataLen = tcbPtr->recv.window;
	    headerFlags &= ~NET_TCP_FIN_FLAG;
	    stats.tcp.recv.packAfterWin++;
	    stats.tcp.recv.byteAfterWin += toDrop;
	}
	tcbPtr->send.updateSeqNum = tcpHdrPtr->seqNum - 1;
	tcbPtr->recv.urgentPtr = tcpHdrPtr->seqNum;
	goto updateWindow;
    }

    /*
     * p. 69
     *
     * Processing for states other than LISTEN or SYN_SENT.
     * First check that at least some bytes of the segment are within 
     * the receive window.  If the segment begins before recv.next,
     * drop leading data (and SYN); if nothing is left, just ack.
     */

    toDrop = tcbPtr->recv.next - tcpHdrPtr->seqNum;
    if (toDrop > 0) {
	if (headerFlags & NET_TCP_SYN_FLAG) {
	    headerFlags &= ~NET_TCP_SYN_FLAG;
	    tcpHdrPtr->seqNum++;

	    if (tcpHdrPtr->urgentOffset > 1) {
		tcpHdrPtr->urgentOffset--;
	    } else {
		headerFlags &= ~NET_TCP_URG_FLAG;
	    }
	    toDrop--;
	}

	/*
	 * If we need to drop more data than what's in the segment, then
	 * drop the segment after ACKing it.
	 */
	if ((toDrop > dataLen) ||
	    ((toDrop == dataLen) && ((headerFlags & NET_TCP_FIN_FLAG) == 0))) {

	    stats.tcp.recv.dupPack++;
	    stats.tcp.recv.dupByte += dataLen;
	    /*
	     * If the segment is just one to the left of the window,
	     * check two special cases:
	     * 1. Don't toss RST in response to 4.2-style keepalive.
	     * 2. If the only thing to drop is a FIN, we can drop
	     *    it, but check the ACK or we will get into FIN
	     *    wars if our FINs crossed (both CLOSING).
	     * In either case, send an ACK to resynchronize,
	     * but keep on processing for RST or ACK.
	     */

	    if (((headerFlags & NET_TCP_FIN_FLAG) && (toDrop == dataLen+1)) 
#ifdef TCP_COMPAT_42
	      || ((headerFlags & NET_TCP_RST_FLAG) && 
		    (tcpHdrPpr->sequm == tcbPtr->recv.next -1))
#endif TCP_COMPAT_42
	    ) {
		toDrop = dataLen;
		headerFlags &= ~NET_TCP_FIN_FLAG;
		tcbPtr->flags |= TCP_ACK_NOW;
	    } else {
		goto dropAfterAck;
	    }
	} else {
	    stats.tcp.recv.partDupPack++;
	    stats.tcp.recv.partDupByte += toDrop;
	}

	/*
	 * Drop data from the front of the segment. This is done by adjusting
	 * the start of the data buffer pointer and length. Since the urgent
	 * pointer is relative to the segment seqNum, it must be adjusted or
	 * cleared.
	 */
	tcpHdrPtr->seqNum += toDrop;
	dataLen		  -= toDrop;
	data		  += toDrop;

	if (tcpHdrPtr->urgentOffset > toDrop) {
	    tcpHdrPtr->urgentOffset -= toDrop;
	} else {
	    headerFlags &= ~NET_TCP_URG_FLAG;
	    tcpHdrPtr->urgentOffset = 0;
	}
    }

    /*
     *
     * If new data are received on a connection after the
     * user processes are gone, then RESET the other end.
     */

    if (!Sock_HasUsers(sockPtr) && 
	    TCP_HAVE_SENT_FIN(tcbPtr->state) &&	(dataLen > 0)) {

	TCPCloseConnection(sockPtr, tcbPtr);
	sockPtr = NULL;
	tcbPtr = NULL;
	stats.tcp.recv.afterClose++;
	goto dropWithReset;
    }

    /*
     * If the segment ends after the window, drop trailing data (and
     * PUSH and FIN bits, too). If there's no data left, then just ACK.
     */

    toDrop = (tcpHdrPtr->seqNum + dataLen) - 
			    (tcbPtr->recv.next + tcbPtr->recv.window);
    if (toDrop > 0) {
	stats.tcp.recv.packAfterWin++;
	if (toDrop >= dataLen) {
	    stats.tcp.recv.byteAfterWin += dataLen;
	    /*
	     * If a new connection request is received while in the 
	     * TIME_WAIT state, drop the old connection and start over 
	     * if the sequence numbers are above the previous ones.
	     */
	    if ((headerFlags & NET_TCP_SYN_FLAG) &&
		(tcbPtr->state == TIME_WAIT) &&
		TCP_SEQ_GT(tcpHdrPtr->seqNum, tcbPtr->recv.next)) {

		iss = tcbPtr->recv.next + TCP_INIT_SEND_SEQ_INCR ;
		TCPCloseConnection(sockPtr, tcbPtr);
		goto findSock;
	    }
	    /*
	     * If window is closed can only take segments at
	     * window edge, and have to drop data and PUSH from
	     * incoming segments.  Continue processing, but
	     * remember to ack.  Otherwise, drop segment
	     * and ack.
	     */
	    if ((tcbPtr->recv.window == 0) && 
		(tcpHdrPtr->seqNum == tcbPtr->recv.next)) {
		stats.tcp.recv.winProbe++;
		tcbPtr->flags |= TCP_ACK_NOW;
	    } else {
		goto dropAfterAck;
	    }
	} else {
	    stats.tcp.recv.byteAfterWin += toDrop;
	}
	dataLen -= toDrop;
	headerFlags &= ~(NET_TCP_PSH_FLAG|NET_TCP_FIN_FLAG);
    }

    /*
     * p. 70
     *
     * Step 2: Check the RESET bit.
     *
     *
     * If the RESET bit is set, and the tcb is in the 
     *   a) SYN_RECEIVED state:
     *		If passive open, return to LISTEN state.
     *		If active open, inform the user that the connection was refused.
     *   b) ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT states:
     *		Inform the user that the connection was reset, and close 
     *		the tcb.
     *   c) CLOSING, LAST_ACK, TIME_WAIT states:
     *		Close the tcb.
     */

    if (headerFlags & NET_TCP_RST_FLAG) {
	switch (tcbPtr->state) {

	    case SYN_RECEIVED:
		TCPDropConnection(sockPtr, tcbPtr, 
					(ReturnStatus)NET_CONNECT_REFUSED);
		tcbPtr = NULL;
		goto drop;

	    case ESTABLISHED:
	    case FIN_WAIT_1:
	    case FIN_WAIT_2:
	    case CLOSE_WAIT:
		TCPDropConnection(sockPtr, tcbPtr, 
				(ReturnStatus)NET_CONNECTION_RESET);
		tcbPtr = NULL;
		goto drop;

	    case CLOSING:
	    case LAST_ACK:
	    case TIME_WAIT:
		TCPCloseConnection(sockPtr, tcbPtr);
		tcbPtr = NULL;
		goto drop;
	}
    }

    /*
     * p. 71
     *
     * Step 3: check security and precedence.
     *
     * (not implemented.)
     *
     */

    /*
     * p. 71
     *
     * State 4: check the SYN bit.
     *
     * If the SYN is in the window, it is an error so send a RESET and
     * drop the connection.
     */
    if (headerFlags & NET_TCP_SYN_FLAG) {
	TCPDropConnection(sockPtr, 
			tcbPtr, (ReturnStatus)NET_CONNECTION_RESET);
	tcbPtr = NULL;
	goto dropWithReset;
    }

    /*
     * p. 72
     *
     * Step 5: check the ACK bit.
     *
     * If the ACK bit is off, we drop the segment and return.
     */

    if ((headerFlags & NET_TCP_ACK_FLAG) == 0) {
	goto drop;
    }
    
    switch (tcbPtr->state) {

	/*
	 * In the SYN_RECEIVED state, if the ACK acknowledges our SYN 
	 * then we enter the ESTABLISHED state and continue processing, 
	 * otherwise we send an RESET.
	 */

	case SYN_RECEIVED:

	    if (TCP_SEQ_GT(tcbPtr->send.unAck, tcpHdrPtr->ackNum) ||
		TCP_SEQ_GT(tcpHdrPtr->ackNum, tcbPtr->send.maxSent)) {
		goto dropWithReset;
	    }
	    stats.tcp.connects++;
	    Sock_Connected(sockPtr);
	    tcbPtr->state = ESTABLISHED;
	    tcbPtr->maxSegSize= MIN(tcbPtr->maxSegSize, 
					TCPCalcMaxSegSize(tcbPtr));
	    packetPtr->data = data;
	    packetPtr->dataLen = dataLen;
	    (void) Reassemble(sockPtr, tcbPtr, (Net_TCPHeader *)NULL, 
				packetPtr);
	    tcbPtr->send.updateSeqNum = tcpHdrPtr->seqNum - 1;

	    /* Fall into ... */

	/*
	 * In the ESTABLISHED state, ignore duplicate and out-of-range ACKs.
	 * If the ack is in the range:
	 *
	 *     tcbPtr->send.unAck < tcpHdrPtr->ackNum <= tcbPtr->send.maxSent
	 *
	 * then advance tcbPtr->send.unAck to tcpHdrPtr->ackNum and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up-to-date window information, we update our window information.
	 */

	case ESTABLISHED:
	case FIN_WAIT_1:
	case FIN_WAIT_2:
	case CLOSE_WAIT:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:

	    if (TCP_SEQ_LE(tcpHdrPtr->ackNum, tcbPtr->send.unAck)) {
		if (dataLen == 0 && tcpHdrPtr->window == tcbPtr->send.window) {
		    stats.tcp.recv.dupAck++;

		    /*
		     * If we have outstanding data (not a window probe), or
		     * this is a completely duplicate ack (i.e., window info
		     * didn't change), the ack is the biggest we've seen
		     * and we've seen exactly our rexmt threshhold of
		     * them, assume a packet has been dropped and
		     * retransmit it.  Kludge send.next & the congestion
		     * window so we send only this one packet.  If this packet 
		     * fills the only hole in the receiver's seq. space, the 
		     * next real ACK will fully open our window. This means we
		     * have to do the usual slow-start to not overwhelm an
		     * intermediate gateway with a burst of packets.  Leave
		     * here with the congestion window set to allow 2 packets
		     * on the next real ACK and the exp-to-linear thresh
		     * set for half the current window size (since we know
		     * we're losing at the current window size).
		     */
		    if ((tcbPtr->timer[TCP_TIMER_REXMT] == 0) ||
			(tcpHdrPtr->ackNum != tcbPtr->send.unAck)) {
			tcbPtr->dupAcks = 0;
		    } else {
			tcbPtr->dupAcks++;
			if (tcbPtr->dupAcks == tcpReXmtThresh) {
			    TCPSeqNum oldNext;
			    unsigned int win;

			    oldNext = tcbPtr->send.next;
			    win = 
			       MIN(tcbPtr->send.window, tcbPtr->send.congWindow)
				/ 2 / tcbPtr->maxSegSize;

			    if (win < 2) {
				win = 2;
			    }
			    tcbPtr->send.cwSizeThresh = 
					win * tcbPtr->maxSegSize;

			    tcbPtr->timer[TCP_TIMER_REXMT] = 0;
			    tcbPtr->rtt = 0;
			    tcbPtr->send.next = tcpHdrPtr->ackNum;
			    tcbPtr->send.congWindow = tcbPtr->maxSegSize;
			    (void) TCPOutput(sockPtr, 
								tcbPtr);

			    if (TCP_SEQ_GT(oldNext, tcbPtr->send.next)) {
				tcbPtr->send.next = oldNext;
			    }
			    goto drop;
			}
		    }
		} else {
		    tcbPtr->dupAcks = 0;
		}
		break;
	    }
	    tcbPtr->dupAcks = 0;
	    if (TCP_SEQ_GT(tcpHdrPtr->ackNum, tcbPtr->send.maxSent)) {
		stats.tcp.recv.ackTooMuch++;
		goto dropAfterAck;
	    }
	    acked = tcpHdrPtr->ackNum - tcbPtr->send.unAck;
	    stats.tcp.recv.ackPack++;
	    stats.tcp.recv.ackByte += acked;

	    /*
	     * If the transmit timer is running and the timed sequence
	     * number was ACKed, update the smoothed round trip time.
	     * Since we now have an rtt measurement, cancel the timer
	     * backoff (c.f., Phil Karn's retransmit alg.). Recompute
	     * the initial retransmit timer.
	     */
	    if ((tcbPtr->rtt != 0) && 
		TCP_SEQ_GT(tcpHdrPtr->ackNum, tcbPtr->rtseq)) {

		stats.tcp.rttUpdated++;

		if (tcbPtr->srtt != 0) {
		    register int delta;

		    /*
		     * The smoothed rtt is stored as fixed point with 3 bits
		     * after the binary point (i.e., scaled by 8).
		     * The following magic is equivalentto the smoothing 
		     * algorithm in RFC793 with an alpha of .875
		     *    srtt = rtt/8 + srtt*7/8 
		     * (in fixed point). Adjust the round-trip time to the
		     * origin of 0.
		     */

		    delta = tcbPtr->rtt - 1 - (tcbPtr->srtt >>3);
		    tcbPtr->srtt += delta;
		    if (tcbPtr->srtt <= 0) {
			tcbPtr->srtt = 1;
		    }
		    /*
		     * We accumulate a smoothed RTT variance (actually, a
		     * smoothed mean difference), then set the retransmit
		     * timer to smoothed RTT + 2 times the smoothed
		     * variance.  rttvar is stored as fixed point with 2
		     * bits after the binary point (scaled by 4).  The
		     * following is equivalent to RFC793 smoothing with an
		     * alpha of .75 (rttvar = rttvar*3/4 + |delta| / 4).
		     * This replaces RFC793's wired-in beta.
		     */
		    if (delta < 0) {
			delta = -delta;
		    }
		    delta -= (tcbPtr->rttvar >> 2);
		    tcbPtr->rttvar += delta;
		    if (tcbPtr->rttvar <= 0) {
			tcbPtr->rttvar = 1;
		    }
		} else {
		    /* 
		     * No RTT measurement yet - use the unsmoothed RTT.
		     * Set the variance to half the RTT (so our first
		     * retransmit happens at 2*RTT).
		     */
		    tcbPtr->srtt = tcbPtr->rtt << 3;
		    tcbPtr->rttvar = tcbPtr->rtt << 1;
		}
		tcbPtr->rtt = 0;
		tcbPtr->rxtshift = 0;
		TCP_TIMER_RANGESET(tcbPtr->rxtcur, 
			((tcbPtr->srtt >> 2) + tcbPtr->rttvar) >> 1,
			TCP_MIN_REXMT_TIME, TCP_MAX_REXMT_TIME);
	    }

	    /*
	     * If all outstanding data is acked, stop retransmit
	     * timer and remember to restart (more output or persist).
	     * If there is more data to be acked, restart retransmit
	     * timer.
	     */
	    if (tcpHdrPtr->ackNum == tcbPtr->send.maxSent) {
		tcbPtr->timer[TCP_TIMER_REXMT] = 0;
		needOutput = TRUE;
	    } else if (tcbPtr->timer[TCP_TIMER_PERSIST] == 0) {
		tcbPtr->timer[TCP_TIMER_REXMT] = tcbPtr->rxtcur;
	    }

	    /*
	     * When new data is acked, open the congestion window.
	     * If the window gives us less than send.cwSizeThresh packets
	     * in flight, open exponentially (maxSegSize per packet).
	     * Otherwise open linearly (maxSegSize per window,
	     * or maxSegSize^2 / congWindow per packet).
	     */
	    {
		unsigned int incr = tcbPtr->maxSegSize;

		if (tcbPtr->send.congWindow > tcbPtr->send.cwSizeThresh) {
		    incr = MAX(incr * incr / tcbPtr->send.congWindow, 1);
		}
		tcbPtr->send.congWindow = 
		    MIN(tcbPtr->send.congWindow + incr, NET_IP_MAX_PACKET_SIZE);
	    }
	    { 
		int used;

		used = Sock_BufSize(sockPtr, SOCK_SEND_BUF,
						SOCK_BUF_USED);
		if (acked > used) {
		    tcbPtr->send.window -= used;
		    Sock_BufRemove(sockPtr, SOCK_SEND_BUF, 
						used);
		    ourFinIsAcked = TRUE;
		} else {
		    Sock_BufRemove(sockPtr, SOCK_SEND_BUF,
					acked);
		    tcbPtr->send.window -= acked;
		    ourFinIsAcked = FALSE;
		}
		/*
		 * Wakeup the waiting writer because there's more room
		 * in the buffer now.
		 */
		if (ips_Debug) {
		    (void) fprintf(stderr,
				"TCP Input: window opened for writer\n");
		}
		Sock_NotifyWaiter(sockPtr, FS_WRITABLE);
	    }

	    tcbPtr->send.unAck = tcpHdrPtr->ackNum;
	    if (TCP_SEQ_LT(tcbPtr->send.next, tcbPtr->send.unAck)) {
		tcbPtr->send.next = tcbPtr->send.unAck;
	    }

	    switch (tcbPtr->state) {

		/*
		 * In the FIN_WAIT_1 state, in addition to the processing
		 * for the ESTABLISHED state, if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */

		case FIN_WAIT_1:
		    if (ourFinIsAcked) {

			/*
			 * If we can't receive any more data, then the
			 * closing user can proceed.  Starting the timer
			 * is contrary to the specification, but if we
			 * don't get a FIN we'll hang forever.
			 */

			if (Sock_IsRecvStopped(sockPtr)) {
			    Sock_Disconnected(sockPtr);
			    tcbPtr->timer[TCP_TIMER_2MSL] = tcpMaxIdle;;
			}
			tcbPtr->state = FIN_WAIT_2;
		    }
		    break;

		/*
		 * In the CLOSING state, in addition to the processing for
		 * the ESTABLISHED state, if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore the
		 * segment.
		 */
		case CLOSING:
		    if (ourFinIsAcked) {
			tcbPtr->state = TIME_WAIT;
			TCPCancelTimers(tcbPtr);
			tcbPtr->timer[TCP_TIMER_2MSL] = 2 * TCP_MSL_TIME;
			Sock_Disconnected(sockPtr);
		    }
		    break;

		/*
		 * The only thing that can arrive in the LAST_ACK state
		 * is an acknowledgment of our FIN.  If our FIN is now
		 * acknowledged, delete the TCB, enter the closed state
		 * and return.
		 */
		case LAST_ACK:
		    if (ourFinIsAcked) {
			TCPCloseConnection(sockPtr, tcbPtr);
			tcbPtr = NULL;
		    }
		    goto drop;

		/*
		 * In the TIME_WAIT state, the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TIME_WAIT:
		    tcbPtr->timer[TCP_TIMER_2MSL] = 2 * TCP_MSL_TIME;
		    goto dropAfterAck;
	    }

	default:
	    break;

    }

updateWindow:

    /*
     * p. 72
     *
     * Update window information. Don't look at the info if the ACK is 
     * missing (TAC's send garbage on the first SYN).
     */

    if ((headerFlags & NET_TCP_ACK_FLAG) &&
	 (TCP_SEQ_LT(tcbPtr->send.updateSeqNum, tcpHdrPtr->seqNum) ||
	  tcbPtr->send.updateSeqNum == tcpHdrPtr->seqNum &&
	     (TCP_SEQ_LT(tcbPtr->send.updateAckNum, tcpHdrPtr->ackNum) ||
	      tcbPtr->send.updateAckNum == tcpHdrPtr->ackNum && 
	      tcpHdrPtr->window > tcbPtr->send.window
	     )
	  )
	) {

	/* 
	 * Keep track of pure window updates.
	 */
	if (dataLen == 0 &&
	    tcbPtr->send.updateAckNum == tcpHdrPtr->ackNum && 
	    tcpHdrPtr->window > tcbPtr->send.window) {
	    stats.tcp.recv.winUpd++;
	}
	tcbPtr->send.window	  = tcpHdrPtr->window;
	tcbPtr->send.updateSeqNum = tcpHdrPtr->seqNum;
	tcbPtr->send.updateAckNum = tcpHdrPtr->ackNum;
	if (tcbPtr->send.window > tcbPtr->send.maxWindow) {
	    tcbPtr->send.maxWindow = tcbPtr->send.window;
	}
	needOutput = TRUE;
    }

    /*
     * p. 73
     *
     * Step 6: check the URG bit.
     *
     * Process segments with urgent data.
     */

    if ((headerFlags & NET_TCP_URG_FLAG) && 
	(tcpHdrPtr->urgentOffset != 0) &&
	!TCP_HAVE_RECVD_FIN(tcbPtr->state)) {

	/*
	 * This is a kludge, but if we receive and accept random urgent
	 * pointers, we'll crash in the read routine.  It's hard to
	 * imagine someone actually wanting to send this much urgent data.
	 */

	if ((tcpHdrPtr->urgentOffset + 
	     Sock_BufSize(sockPtr, SOCK_RECV_BUF, 
				SOCK_BUF_USED)) > 
		Sock_BufSize(sockPtr,
				SOCK_RECV_BUF, SOCK_BUF_MAX_SIZE)) {
	    tcpHdrPtr->urgentOffset = 0;		/* XXX */
	    headerFlags &= ~NET_TCP_URG_FLAG;		/* XXX */
	} else {

	    /*
	     * If this segment advances the known urgent pointer, then
	     * mark the data stream.  This should not happen in
	     * CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT states since a
	     * FIN has been received from the remote side.  In these
	     * states, we ignore the URG.
	     *
	     * According to RFC1011 (Assigned Internet Protocols), the urgent
	     * pointer points to the last octet of urgent data.  We
	     * continue, however, to consider it to indicate the first
	     * octet of data past the urgent section as the original spec
	     * states.
	     */

	    if (TCP_SEQ_GT(tcpHdrPtr->seqNum + tcpHdrPtr->urgentOffset, 
				tcbPtr->recv.urgentPtr)) {

		tcbPtr->recv.urgentPtr = tcpHdrPtr->seqNum + 
					     tcpHdrPtr->urgentOffset;
		
		tcbPtr->urgentBufPos = 
			Sock_BufSize(sockPtr, 
			        SOCK_RECV_BUF, SOCK_BUF_USED) + 
				tcbPtr->recv.urgentPtr - tcbPtr->recv.next - 1;
		if (tcbPtr->urgentBufPos == 0) {
		    Sock_UrgentDataNext(sockPtr);
		}
		Sock_HaveUrgentData(sockPtr);
		tcbPtr->flags &= ~(TCP_HAVE_URGENT_DATA | TCP_HAD_URGENT_DATA);
		stats.tcp.recv.urgent++;
	    }

	    /*
	     * Remove urgent data from the segment so it doesn't get 
	     * presented to the user in the receive buffer. (It is still 
	     * reflected in the segment length for sequencing purposes.) 
	     * This can happen independent of advancing the URG pointer, 
	     * but if two URG's are pending at once, some urgent data may 
	     * creep in... ick.
	     */

	    if ((tcpHdrPtr->urgentOffset <= dataLen) &&
		!Sock_IsOptionSet(sockPtr, 
					NET_OPT_OOB_INLINE)) {

		int	urgentOffset = tcpHdrPtr->urgentOffset -1;

		if (urgentOffset > dataLen) {
		    (void) fprintf(stderr, 
			  "Warning: TCP Input: urgent data not in segment\n");
		} else {
		    Address urgData = data;
		    urgData	  	+= urgentOffset;
		    tcbPtr->urgentData	 = *urgData;
		    tcbPtr->flags 	|= TCP_HAVE_URGENT_DATA;
		    bcopy( urgData+1, urgData, dataLen - urgentOffset -1);
		    dataLen--;
		    /*
		     * Make sure an ACK is sent if the packet just contained
		     * urgent data. Step 7 is not done if dataLen is 0 so
		     * we must set the ACK flag here.
		     */
		    if ((dataLen == 0) &&
			(tcpHdrPtr->seqNum == tcbPtr->recv.next)) {
			tcbPtr->recv.next++;
			tcbPtr->flags |= TCP_DELAY_ACK;
			stats.tcp.recv.urgentOnly++;
		    }
		}
	    }
	}
    } else {
	/*
	 * If no urgent data are expected, pull receive urgent pointer 
	 * along with the receive window.
	 */
	if (TCP_SEQ_GT(tcbPtr->recv.next, tcbPtr->recv.urgentPtr)) {
	    tcbPtr->recv.urgentPtr = tcbPtr->recv.next;
	}
    }


    /*
     * p. 74
     *
     * Step 7:  Process the segment data.
     *
     * The data is merged into the TCP sequencing queue, and acknowledgment 
     * of receipt is arranged, if necessary.  This process logically involves 
     * adjusting tcbPtr->recv.window as data is presented to the user.  If 
     * a FIN has already been received on this connection then we just ignore 
     * the data.
     */

    if ( ((dataLen != 0) || (headerFlags & NET_TCP_FIN_FLAG)) &&
	!TCP_HAVE_RECVD_FIN(tcbPtr->state)) {

	int len;

	/*
	 * Check for the common case before calling Reassemble(): the new
	 * segment is the next one to be received on an established
	 * connection and the reassembly queue is empty.
	 */

	if ((tcpHdrPtr->seqNum == tcbPtr->recv.next) &&
	    List_IsEmpty(&tcbPtr->reassList) &&
	    (tcbPtr->state == ESTABLISHED)) {

	    int bytesWritten;

	    tcbPtr->recv.next += dataLen; 
	    headerFlags = tcpHdrPtr->flags & NET_TCP_FIN_FLAG; 
	    stats.tcp.recv.pack++;
	    stats.tcp.recv.byte += dataLen;
	    if ((Sock_BufAppend(sockPtr, 
			    SOCK_RECV_BUF, FALSE, dataLen, data, 
			    packetPtr->base, (Net_InetSocketAddr *) NULL,
			    &bytesWritten) != SUCCESS)
		    || (bytesWritten != dataLen)) {
		(void) fprintf(stderr, 
			"Warning: (TCP)Input: append to recv buffer failed\n");
	    }

	    if (ips_Debug) {
		(void) fprintf(stderr, 
			"TCP Input: appended %d bytes: '%.*s'\n", 
				dataLen, MIN(dataLen, 20), data);
	    }
	    Sock_NotifyWaiter(sockPtr, FS_READABLE);
	    tcbPtr->flags |= TCP_DELAY_ACK;
	} else {
	    /*
	     * ACK immediately when segments are out of order so fast 
	     * retransmission can work.
	     */
	    tcbPtr->flags |= TCP_ACK_NOW;
	    packetPtr->data = data;
	    packetPtr->dataLen = dataLen;
	    headerFlags = Reassemble(sockPtr, tcbPtr, tcpHdrPtr, packetPtr);
	} 
	/*
	 * Don't free the packet memory when we leave this routine because
	 * the packet is now on the reassembly queue or in the socket's
	 * receive buffer.
	 */
	freePacket = FALSE;

	/*
	 * Note the amount of data that peer has sent into our window, 
	 * in order to estimate the sender's buffer size.
	 */
	len = Sock_BufSize(sockPtr, SOCK_RECV_BUF, 
		    SOCK_BUF_MAX_SIZE) -
		    (tcbPtr->recv.next - tcbPtr->recv.advtWindow);
	if (len > tcbPtr->recv.maxWindow) {
	    tcbPtr->recv.maxWindow = len;
	}
    } else {
	headerFlags &= ~NET_TCP_FIN_FLAG;
    }


    /*
     * p. 75
     *
     * Step 8: check the FIN bit.
     *
     * If a FIN is received, ACK the FIN and let the user know that 
     * the connection is closing.
     */

    if (headerFlags & NET_TCP_FIN_FLAG) {
	if (!TCP_HAVE_RECVD_FIN(tcbPtr->state)) {
	    Sock_StopRecv(sockPtr);
	    tcbPtr->flags |= TCP_ACK_NOW;
	    tcbPtr->recv.next++;
	}
	switch (tcbPtr->state) {

	    /*
	     * In the SYN_RECEIVED and ESTABLISHED states, enter the 
	     * CLOSE_WAIT state.
	     */
	    case SYN_RECEIVED:
	    case ESTABLISHED:
		tcbPtr->state = CLOSE_WAIT;
		break;

	    /*
	     * If still in the FIN_WAIT_1 state, our FIN has not been acked so
	     * enter the CLOSING state.
	     */
	    case FIN_WAIT_1:
		tcbPtr->state = CLOSING;
		break;

	    /*
	     * In the FIN_WAIT_2 state, enter the TIME_WAIT state and start
	     * the time-wait timer, turning off the other standard timers.
	     */
	    case FIN_WAIT_2:
		tcbPtr->state = TIME_WAIT;
		TCPCancelTimers(tcbPtr);
		tcbPtr->timer[TCP_TIMER_2MSL] = 2 * TCP_MSL_TIME;
		Sock_Disconnected(sockPtr);
		break;

	    /*
	     * In the TIME_WAIT state, restart the 2*MSL time-wait timer.
	     */
	    case TIME_WAIT:
		tcbPtr->timer[TCP_TIMER_2MSL] = 2 * TCP_MSL_TIME;
		break;
	}
    }

    if (Sock_IsOptionSet(sockPtr, NET_OPT_DEBUG)) {
	TCPTrace(TCP_TRACE_INPUT, oldState, tcbPtr, &saveHeader, dataLen);
    }

    /*
     * Return any desired output.
     */
    if (needOutput || (tcbPtr->flags & TCP_ACK_NOW)) {
	(void) TCPOutput(sockPtr, tcbPtr);
    }

    if (freePacket) {
	free(packetPtr->base);
    }
    return;


dropAfterAck:

    /*
     * Generate an ACK dropping incoming segment if it occupies
     * sequence space, where the ACK reflects our state.
     */
    if (headerFlags & NET_TCP_RST_FLAG) {
	goto drop;
    }
    if (Sock_IsOptionSet(sockPtr, NET_OPT_DEBUG)) {
	TCPTrace(TCP_TRACE_RESPOND, oldState, tcbPtr, &saveHeader, dataLen);
    }
    TCPRespond(sockPtr, tcpHdrPtr, ipHdrPtr, 
			tcbPtr->recv.next, tcbPtr->send.next, NET_TCP_ACK_FLAG);
    free(packetPtr->base);
    return;


dropWithReset:

    /*
     * Generate a RESET, dropping incoming segment.
     * Make ACK acceptable to originator of segment.
     * Don't bother to respond if destination was broadcast.
     */

    if ((headerFlags & NET_TCP_RST_FLAG) || 
	Rte_IsBroadcastAddr(ipHdrPtr->dest)) {
	goto drop;
    }
    if (headerFlags & NET_TCP_ACK_FLAG) {
	TCPRespond(sockPtr,
		       tcpHdrPtr, ipHdrPtr, (TCPSeqNum)0, 
			tcpHdrPtr->ackNum, NET_TCP_RST_FLAG);
    } else {
	if (headerFlags & NET_TCP_SYN_FLAG) {
	    dataLen++;
	}
	TCPRespond(sockPtr, tcpHdrPtr, ipHdrPtr,  
			tcpHdrPtr->seqNum + dataLen, 
			(TCPSeqNum)0, NET_TCP_RST_FLAG|NET_TCP_ACK_FLAG);
    }
    free(packetPtr->base);
    return;


drop:

    /*
     * Drop space held by incoming segment and return.
     */

    if ((tcbPtr != NULL) && 
	(sockPtr != NULL) && 
	Sock_IsOptionSet(sockPtr, NET_OPT_DEBUG)) {
	TCPTrace(TCP_TRACE_DROP, oldState, tcbPtr, &saveHeader, dataLen);
    }
    free(packetPtr->base);
    return;
}



/*
 *----------------------------------------------------------------------
 *
 * CalcFreeSpace --
 *
 *	Calculate amount of space in the receive window.
 *	The Receive window is amount of space in the receive queue,
 *	but not less than the advertised window.
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
CalcFreeSpace(sockPtr, tcbPtr)
    Sock_InfoPtr	sockPtr;
    TCPControlBlock 	*tcbPtr;
{
    int win;

    win = Sock_BufSize(sockPtr, SOCK_RECV_BUF, SOCK_BUF_FREE);
    if (win < 0) { 
	win = 0;
    }
    tcbPtr->recv.window = 
		MAX(win, (int)(tcbPtr->recv.advtWindow - tcbPtr->recv.next));
}


/*
 *----------------------------------------------------------------------
 *
 * ProcessOptions --
 *
 *	Decode the options that follow the TCP header.
 *	There is currently just one option: the maximum segment size.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The maximum segment size of the tcb is updated.
 *
 *----------------------------------------------------------------------
 */

static void
ProcessOptions(len, tcpHdrPtr, tcbPtr)
    int			len;		/* Length of the TCP basic header 
					 * and options. */
    Net_TCPHeader	*tcpHdrPtr;	/* Ptr to start of the TCP header. */
    TCPControlBlock	*tcbPtr;	/* The TCB to be updated. */
{
    register unsigned char *cp;
    int option;
    int optionLen;
    int maxSegSize;

    cp = (unsigned char *) tcpHdrPtr;
    
    for (len -= sizeof(Net_TCPHeader); len > 0; 
	 len -= optionLen, cp += optionLen) {

	option = cp[0];
	if (option == NET_TCP_OPTION_EOL) {
	    break;
	}
	if (option == NET_TCP_OPTION_NOP) {
	    optionLen = 1;
	} else {
	    optionLen = cp[1];
	    if (optionLen <= 0) {
		break;
	    }
	}
	switch (option) {

	    case NET_TCP_OPTION_MAX_SEG_SIZE:
		if (optionLen != 4) {
		    continue;
		}
		if (!(tcpHdrPtr->flags & NET_TCP_SYN_FLAG)) {
		    continue;
		}
		maxSegSize = Net_NetToHostShort(*(unsigned short *)(cp + 2));
		tcbPtr->maxSegSize = MIN(maxSegSize, TCPCalcMaxSegSize(tcbPtr));
		break;

	    default:
		    break;

	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TCPCalcMaxSegSize --
 *
 *	Determines a reasonable value for the maximum segment size.
 *	The value depends on the route to the remote peer.  If
 *	the peer is on the local net, we output a size based on the
 *	maximum size on the local net.  (This includes other subnets on
 *	our net -- see Rte_IsLocal.)  If the peer is remote, return
 *	TCP_MAX_SEG_SIZE, which is on the order of 500 bytes and should
 *	never need to be fragmented by another IP server.
 *
 * Results:
 *	The maximum segment size is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
int
TCPCalcMaxSegSize(tcbPtr)
    TCPControlBlock *tcbPtr;
{
    register Net_IPHeader *ipHdrPtr;
    Rte_NetID		netID;
    int localOutSize;
    int maxOutSize = TCP_MAX_SEG_SIZE;
    int subnet;

    ipHdrPtr = tcbPtr->IPTemplatePtr;
    if (ipHdrPtr != ((Net_IPHeader *) NULL)) {
	if (!Rte_FindOutputNet(ipHdrPtr->dest,  &netID, &localOutSize)) {
	    return(TCP_MAX_SEG_SIZE);
	}
	maxOutSize = localOutSize - sizeof(Net_TCPHeader) -
	    sizeof(Net_IPHeader);
	if (Rte_IsLocalAddr(netID)) {
#ifdef DEBUG_SIZE
	    (void) fprintf(stderr,
			   "TCPCalcMaxSegSize: host is local, size %d\n",
			   maxOutSize);
#endif
	    return(maxOutSize);
	}
	maxOutSize = MIN(maxOutSize, TCP_MAX_SEG_SIZE);
	tcbPtr->send.congWindow = maxOutSize;
#ifdef DEBUG_SIZE
	    (void) fprintf(stderr,
			   "TCPCalcMaxSegSize: host is remote, size %d\n",
			   maxOutSize);
#endif
    } else {
#ifdef DEBUG_SIZE
	(void) fprintf(stderr,
		       "TCPCalcMaxSegSize: no ipHdrPtr\n");
#endif
    }	
    return (maxOutSize);
}




/*
 *----------------------------------------------------------------------
 *
 * Reassemble --
 *
 *	Reassembles out-of-order data segments for a TCB.
 *
 * Results:
 *	If not 0, the TCP header flags of the reassembled data.
 *
 * Side effects:
 *	Data may be appended to the socket's receive buffer.
 *
 *----------------------------------------------------------------------
 */

static unsigned short
Reassemble(sockPtr, tcbPtr, tcpHdrPtr, packetPtr)
    Sock_InfoPtr		sockPtr;	/* Socket of interest. */
    TCPControlBlock 		*tcbPtr;	/* TCB for the socket. */
    register Net_TCPHeader	*tcpHdrPtr;	/* Header of the incoming 
						 * packet. */
    register IPS_Packet		*packetPtr;	/* Descriptor of the incoming
						 * packet. */
{
    register ReassElement	*reassPtr;
    ReassElement	*tempPtr;
    unsigned short	flags;
    Boolean		notifyNeeded;
    List_Links		*listPtr;
    register int	dataLen;

    listPtr = &tcbPtr->reassList;
    dataLen = packetPtr->dataLen;

    /*
     * If we're called with tcpHdrPtr == NULL after entering the ESTABLISHED 
     * state, then force pre-ESTABLISHED data up to user socket.
     */

    if (tcpHdrPtr != (Net_TCPHeader *) NULL) {

	/*
	 * Find a segment which begins after this one does
	 * (the sequence # will be greater than the seq. # of the new
	 * segment).
	 */
	LIST_FORALL(listPtr, (List_Links *)reassPtr) {
	    if (TCP_SEQ_GT(reassPtr->seqNum, tcpHdrPtr->seqNum)) {
		break;
	    }
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If the preceding segment provides all of our data, 
	 * drop the new segment.
	 *
	 * reassPtr now points to the next segment or the end of the list.
	 * If there is a previous segment, the List_Prev of reassPtr should
	 * not be the end of the list.
	 */
	if (!List_IsAtEnd(listPtr, List_Prev((List_Links *)reassPtr))) {
	    register int i;

	    reassPtr = (ReassElement *) List_Prev((List_Links *)reassPtr);

	    /*
	     * Check for overlap of new and old data:
	     *
	     *              new s#
	     *           i: |--|--|
	     *               + 0 -
	     *
	     *     +--- old ---+
	     *    seq#       s#+len
	     *
	     * If i is positive, then the new segment overlaps with
	     * the old. If it's 0 or negative then there's no overlap.
	     */

	    /* conversion to int (in i) handles seq wraparound */
	    i = (reassPtr->seqNum + reassPtr->len) - tcpHdrPtr->seqNum;

	    if (i > 0) {
		/*
		 * Overlap. If i is greater than the length of the new
		 * segment, then the old segment completely overlaps
		 * the new one so it duplicates data and it can be dropped.
		 */
		if (i >= dataLen) {
		    stats.tcp.recv.dupPack++;
		    stats.tcp.recv.dupByte += dataLen;
		    free(packetPtr->base);
		    return(0);
		}
		/*
		 * Partial overlap. Trim data from the end of the new segment.
		 */
		dataLen		-= i;
		packetPtr->data	+= i;
		tcpHdrPtr->seqNum += i;
	    }
	    reassPtr = (ReassElement *) List_Next((List_Links *)reassPtr);
	}
	stats.tcp.recv.ooPack++;
	stats.tcp.recv.ooByte += dataLen;

	/*
	 * Now see if the new segment overlaps succeeding segments.
	 * If so, then trim or delete the old ones.
	 */
	while (!List_IsAtEnd(listPtr, (List_Links *)reassPtr)) {
	    register int i;

	    /*
	     * Determine how much overlap there is. If i is 0 or negative
	     * then, there's no overlap. If i is greater than the length
	     * of the old segment, then the new segment completely covers it.
	     */
	    i = (tcpHdrPtr->seqNum + dataLen) - reassPtr->seqNum;
	    if (i <= 0) {
		break;
	    }
	    if (i < reassPtr->len) {
		/*
		 * Partial overlap. Trim data from the front of the old segment.
		 */
		reassPtr->seqNum += i;
		reassPtr->len	-= i;
		reassPtr->data	+= i;
		break;
	    }
	    /*
	     * Complete overlap. Remove the old segment.
	     */
	    tempPtr = (ReassElement *)List_Next((List_Links *)reassPtr);
	    List_Remove((List_Links *)reassPtr);
	    free(reassPtr->base);
	    free((char *) reassPtr);
	    reassPtr = tempPtr;
	}

	/*
	 * Stick new segment in its place.
	 */
	tempPtr = (ReassElement *) malloc(sizeof(ReassElement));
	List_InitElement((List_Links *) tempPtr);
	tempPtr->len = dataLen;
	tempPtr->data = packetPtr->data;
	tempPtr->base = packetPtr->base;
	tempPtr->seqNum = tcpHdrPtr->seqNum;
	tempPtr->flags = tcpHdrPtr->flags;
	List_Insert((List_Links *)tempPtr, LIST_BEFORE((List_Links *)reassPtr));
    }

    /*
     * Go through the reassembly queue and append the first contigous chunk 
     * of the segment to the socket receive buffer. Stop when a gap is found
     * or the end of the list is reached.
     */

    if (!TCP_HAVE_RECVD_SYN(tcbPtr->state)) {
	return(0);
    }
    reassPtr = (ReassElement *) List_First(listPtr);
    if (List_IsAtEnd(listPtr, (List_Links *)reassPtr) ||
	(reassPtr->seqNum != tcbPtr->recv.next)) {
	return(0);
    }
    if ((tcbPtr->state == SYN_RECEIVED) && (dataLen != 0)) {
	return(0);
    }

    notifyNeeded = FALSE;
    do {
	tcbPtr->recv.next += reassPtr->len;
	flags = reassPtr->flags & NET_TCP_FIN_FLAG;
	tempPtr = (ReassElement *)List_Next((List_Links *) reassPtr);
	List_Remove((List_Links *)reassPtr);
	if (Sock_IsRecvStopped(sockPtr)) {
	    free(reassPtr->base);
	} else {
	    int amt;

	    if ((Sock_BufAppend(sockPtr,
			    SOCK_RECV_BUF, FALSE, reassPtr->len,
			    reassPtr->data, reassPtr->base, 
			    (Net_InetSocketAddr *) NULL,
			    &amt) != SUCCESS) || (amt != reassPtr->len)) {
		if (amt == 0) {
		    free(reassPtr->base);
		}
	    }
	    if (ips_Debug) {
		(void) fprintf(stderr, 
			"TCP Reassemble: appended %d bytes: '%.*s'\n",
				amt, MIN(amt, 20), reassPtr->data);
				
	    }
	    notifyNeeded = TRUE;
	}
	free((char *) reassPtr);
	reassPtr = tempPtr;
    } while (!List_IsAtEnd(listPtr, (List_Links *)reassPtr) && 
	    (reassPtr->seqNum == tcbPtr->recv.next));

    if (notifyNeeded) {
	Sock_NotifyWaiter(sockPtr, FS_READABLE);
    }
    return(flags);
}


/*
 *----------------------------------------------------------------------
 *
 * TCPCleanReassList --
 *
 *	Removes all segments on the reassembly list of a TCP control block.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory for the reassembly information and packets are deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TCPCleanReassList(tcbPtr)
    TCPControlBlock 	*tcbPtr;	/* TCB containing a reassembly Q. */
{
    List_Links	*ptr;
    List_Links	*tempPtr;

    LIST_FORALL(&tcbPtr->reassList, ptr) {
	tempPtr = List_Next(ptr);
	List_Remove(ptr);
	free((char *) ((ReassElement *)ptr)->base);
	free((char *) ptr);
	ptr = tempPtr;
    }
}
