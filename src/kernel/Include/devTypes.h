/*
 * devTypes.h --
 *
 *	This file declares the major device types used in Sprite.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/kernel/Cvsroot/kernel/dev/devTypes.h,v 9.15 92/10/26 13:41:04 mgbaker Exp $ SPRITE (Berkeley)
 */

#ifndef _DEVTYPES
#define _DEVTYPES

/* constants */

/*
 * Device types:
 *
 *	DEV_TERM		Terminal devices, like the console
 *	DEV_SYSLOG		The system log device
 *	DEV_MEMORY		Null device and kernel memory area.
 *	DEV_NET			Raw ethernet device - unit number is protocol.
 *	DEV_KEYBOARD		Keyboard
 *	DEV_SCSI_DISK		Disk on the SCSI bus
 *	DEV_SCSI_TAPE		Tape drive on the SCSI bus
 *	DEV_XYLOGICS		Xylogics 450 controller
 *	DEV_SCSI_HBA		Raw SCSI HBA device driver
 *	DEV_RAID		Raw interface to disk array
 *	DEV_DEBUG		For debugging disk arrays?
 *	DEV_MOUSE		Mouse and keystroke info for window systems
 *	DEV_AUDIO		Audio device
 *	DEV_SCSI_ROBOT		Tape robot on the SCSI bus.
 *      DEV_SMEM                Kernel memory image
 *	DEV_CLIENT_STATE	Device for tracking clients of server.
 *	DEV_STDFB		"Standard" frame buffer. The idea is that this
 *				device behaves the same on machine types,
 *				unlike the mouse or graphics device.
 *
 *
 * NOTE: These numbers correspond to the major numbers for the devices
 * in /dev. Do not change them unless you redo makeDevice for all the devices
 * in /dev.
 *
 */

#define	DEV_TERM		0
#define	DEV_SYSLOG		1
#define	DEV_SCSI_WORM		2
#define	DEV_PLACEHOLDER_2	3
#define	DEV_SCSI_DISK		4
#define	DEV_SCSI_TAPE		5
#define	DEV_MEMORY		6
#define	DEV_XYLOGICS		7
#define	DEV_NET			8
#define	DEV_SCSI_HBA		9
#define	DEV_RAID		10
#define	DEV_DEBUG		11
/* Number 12 taken below. */
#define DEV_PLACEHOLDER_3	13      /* for ds3100 unused graphics device */
#define DEV_SMEM                14
/* #define DEV_ZDC_DISK		14	Only on sequent symmetry */
#define DEV_AUDIO		15
#define DEV_VMELINK		16
#define DEV_STDFB 		17
#define DEV_ATC			18      /* used by sun4 only */
#define DEV_PLACEHOLDER_4       18      /* used by decstations */
#define DEV_PLACEHOLDER_5	19
#define DEV_SCSI_ROBOT		20
#define	DEV_XBUS		21
#define	DEV_CLIENT_STATE	22


#if (!defined(ds3100)) && (!defined(ds5000))
#define DEV_MOUSE		12
#define DEV_GRAPHICS		13
#else
#define	DEV_CONSOLE		0
#define DEV_GRAPHICS		9
/*
 * Unit numbers for the graphics device.
 */
#define DEV_MOUSE		0
#define DEV_XCONS		1

/*
 * SCSI HBA's attached to the system.
 */
#define DEV_SII_HBA	0
#endif 




/*
 * The following device types are defined for SPUR as of 7/15/89.  They
 * should be changed so that don't overlap the device types above.
 * Also, why do two different devices have the same number?
 */

#define DEV_CC			9
#define DEV_PCC			9

/*
 * SCSI HBA's attached to the system.
 */

#define	DEV_SCSI3_HBA	0
#define	DEV_SCSI0_HBA	1
#define	DEV_JAGUAR_HBA  2

/*
 * The following exists only on the sparc station.
 */
#define DEV_SCSIC90_HBA	0

#endif /* _DEVTYPES */

