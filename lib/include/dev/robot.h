/* 
 * robot.h --
 *
 *	Definitions and macros for the Exabyte EXB-120 robot device.
 *
 * Copyright 1992 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that this copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/lib/include/dev/RCS/robot.h,v 1.2 92/03/06 12:54:28 mani Exp $ SPRITE (Berkeley)
 */

#ifndef _ROBOT
#define _ROBOT

#include <machparam.h>

/*
 * The following structure is a parameter to an Fs_IOControl call.
 * Obviously, not all the fields will be used for every call. For
 * example, a MoveMedium command will only use sourceAddr and
 * destAddr, and perhaps eePos.
 */

typedef struct Dev_RobotCommand {
    unsigned int	sourceAddr; 	/* Address of tape to be moved. */
    unsigned int	destAddr;  	/* Destination of robot arm or of
					 * tape to be moved. */
    unsigned int	elemAddr;	/* Starting address of elements 
					 * for the InitializeElemStatus 
					 * command. */
    unsigned int	numElements;	/* Number of elements to be 
					 * initialized. */
    unsigned int	allocLength;	/* Number of bytes allocated for
					 * additional data. Command dependent. */
    unsigned char 	eePos;		/* What to do with the Entry/Exit port.
					 * See the documentation on the Move/Medium
					 * command. */
    unsigned char 	prevAllow;	/* For the Prevent/Allow removal command. */
    unsigned char	range;		/* The range of elements to be scanned.
					 * See the InitializeElemStatus command. */
    unsigned char	volTag;		/* Used with the ReadElemStatus command. */
    unsigned char	elemTypeCode;  	/* Used with the ReadElemStatus command. */
    unsigned char	pageControl;	/* Used with the ModeSense command. */
    Address 		dataPtr;	/* Address of data to be returned from
					 * the device. */
    unsigned int 	dataLength;	/* Length of the data. */
    unsigned int 	savedPage;	/* Whether the ModeSelect changes are
					 * stored in nonvolatile memory or not. */
    unsigned char	mesgDisplay;	/* For the ModeSelect command.
					 * 0 == Flashing 8-character display.
					 * 1 == Steady 8-character display.
					 * 2 == Scrolling display. */
    char       		*mesgString;	/* For the ModeSelect command.
					 * The message to be displayed on
					 * the front of the EXB-120. */
} Dev_RobotCommand;



typedef struct ExbRobotInquiryData {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char type;     	/* Peripheral device type */
    unsigned char rmb:1;        /* Removable Medium bit. */
    unsigned char qualifier:7;  /* Device type qualifier. */
    unsigned char version;      /* Version info. */
    unsigned char reserved:4;   /* reserved. */
    unsigned char format:4;     /* Response format. */
#else /* BYTE_ORDER == LITTLE_ENDIAN */
    unsigned char type;		/* Peripheral device type. */
    unsigned char qualifier:7;	/* Device Type qualifier. */
    unsigned char rmb:1;	/* Removable Medium bit. */
    unsigned char version;	/* Version info. */
    unsigned char format:4;	/* Response format */
    unsigned char reserved:4;	/* reserved. */
#endif
    unsigned char length;         /* Additional length of data returned. */
    unsigned char reserved2[3];   /* More reserved and not supported. */
    char          vendorID[8];  /* Vector identification. */
    char          productID[16]; /* Product identification. */
    char          revLevel[4]; /* Firmware identification. */
    unsigned char reserved3[20];  /* More reserved. */
} ExbRobotInquiryData; 


typedef struct ExbRobotSenseData {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char valid:1;	/* Reserved. */
    unsigned char errorClass:3;	/* Binary 111 on EXB. */
    unsigned char errorCode:4;	/* 0 on EXB. */
    unsigned char segmentNum;	/* 0 on EXB. */
    unsigned char filemark:1;	/* The following three are 0 on EXB. */
    unsigned char eom:1;
    unsigned char ili:1;
    unsigned char reserved:1;
    unsigned char senseKey:4;	/* 0x0 : No Sense
				 * 0x2 : Not Ready
				 * 0x4 : Hardware Error
				 * 0x5 : Illegal Request
				 * 0x6 : Unit Attention
				 * 0xb : Aborted Command
				 */
    unsigned char infoBytes[4];
    unsigned char addlSenseLen;	/* Number of additional bytes of
				 * sense data.
				 */
    unsigned char commandSpecific[4];
    unsigned char addlSenseCode;
    unsigned char addlSenseCodeQualfr;
    unsigned char fruCode;
    unsigned char sksv:1;
    unsigned char senseKeySpecfc1:7;
    unsigned char senseKeySpecfc2[2];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
    unsigned char errorCode:4;
    unsigned char errorClass:3;
    unsigned char valid:1;
    unsigned char segmentNum;
    unsigned char senseKey:4;
    unsigned char reserved:1;
    unsigned char ili:1;
    unsigned char eom:1;
    unsigned char filemark:1;
    unsigned char infoBytes[4];
    unsigned char addlSenseLen;
    unsigned char commandSpecific[4];
    unsigned char addlSenseCode;
    unsigned char addlSenseCodeQualfr;
    unsigned char fruCode;
    unsigned char senseKeySpecfc1:7;
    unsigned char sksv:1;
    unsigned char senseKeySpecfc2[2];
#endif
} ExbRobotSenseData;


#define IOC_ROBOT_INIT_ELEM_STATUS 	62
#define IOC_ROBOT_READ_ELEM_STATUS	63
#define IOC_ROBOT_INQUIRY		64
#define IOC_ROBOT_REQ_SENSE		65
#define IOC_ROBOT_MOVE_MEDIUM		66
#define IOC_ROBOT_POS_ELEM		67
#define IOC_ROBOT_MODE_SEL		68
#define IOC_ROBOT_MODE_SENSE		69
#define IOC_ROBOT_NO_OP			70
#define IOC_ROBOT_PREV_REMOVAL		71
#define IOC_ROBOT_DISPLAY		72

#endif /* _ROBOT */



