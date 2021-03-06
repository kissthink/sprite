/*
 * fb.h --
 *
 *	Declarations of things to do with the frame buffer device.
 *
 * Copyright 1990 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/lib/include/sys/RCS/fb.h,v 1.2 90/06/21 14:53:02 mgbaker Exp $ SPRITE (Berkeley)
 */

#ifndef _FBDEV
#define _FBDEV

#ifndef ASM

/* 
 * Set/Get attributes 
 */
#define	FB_ATTR_NDEVSPECIFIC	8	/* no. of device specific values */
#define	FB_ATTR_NEMUTYPES	4	/* no. of emulation types */

/* 
 * Video control 
 * (the unused bits are reserved for future use)
 */
#define	FBVIDEO_OFF	0
#define	FBVIDEO_ON	1

/* frame buffer type codes */
#define	FBTYPE_SUN1BW		0	/* Multibus mono */
#define	FBTYPE_SUN1COLOR	1	/* Multibus color */
#define	FBTYPE_SUN2BW		2	/* memory mono */
#define	FBTYPE_SUN2COLOR	3	/* color w/rasterop chips */
#define	FBTYPE_SUN2GP		4	/* GP1/GP2 */
#define	FBTYPE_SUN5COLOR	5	/* RoadRunner accelerator */
#define	FBTYPE_SUN3COLOR	6	/* memory color */
#define	FBTYPE_MEMCOLOR		7	/* memory 24-bit */
#define	FBTYPE_SUN4COLOR	8	/* memory color w/overlay */

#define	FBTYPE_NOTSUN1		9	/* reserved for customer */
#define	FBTYPE_NOTSUN2		10	/* reserved for customer */
#define	FBTYPE_NOTSUN3		11	/* reserved for customer */

#define	FBTYPE_SUNFAST_COLOR	12	/* accelerated 8bit */
#define	FBTYPE_SUNROP_COLOR	13	/* MEMCOLOR with rop h/w */
#define	FBTYPE_SUNFB_VIDEO	14	/* Simple video mixing */
#define	FBTYPE_RESERVED5	15	/* reserved, do not use */
#define	FBTYPE_RESERVED4	16	/* reserved, do not use */
#define	FBTYPE_RESERVED3	17	/* reserved, do not use */
#define	FBTYPE_RESERVED2	18	/* reserved, do not use */
#define	FBTYPE_RESERVED1	19	/* reserved, do not use */

#define	FBTYPE_LASTPLUSONE	20	/* max number of fbs (change as add) */

/* data structures */
/*
 * Frame buffer descriptor.
 * Returned by FBIOGTYPE ioctl on frame buffer devices.
 */
typedef struct	fbtype {
	int	fb_type;	/* as defined above */
	int	fb_height;	/* in pixels */
	int	fb_width;	/* in pixels */
	int	fb_depth;	/* bits per pixel */
	int	fb_cmsize;	/* size of color map (entries) */
	int	fb_size;	/* total size in bytes */
} fbtype;


/*
 * General purpose structure for passing info in and out of frame buffers
 * (used for gp1)
 */
typedef struct	fbinfo {
	int		fb_physaddr;	/* physical frame buffer address */
	int		fb_hwwidth;	/* fb board width */
	int		fb_hwheight;	/* fb board height */
	int		fb_addrdelta;	/* phys addr diff between boards */
	unsigned char	*fb_ropaddr;	/* fb va thru kernelmap */
	int		fb_unit;	/* minor devnum of fb */
} fbinfo;



/*
 * Color map I/O
 */
typedef struct	fbcmap {
	int		index;		/* first element (0 origin) */
	int		count;		/* number of elements */
	unsigned char	*red;		/* red color map elements */
	unsigned char	*green;		/* green color map elements */
	unsigned char	*blue;		/* blue color map elements */
} fbcmap;


typedef struct fbsattr {
	int	flags;			/* misc flags */
#define	FB_ATTR_AUTOINIT	1	/* emulation auto init flag */
#define	FB_ATTR_DEVSPECIFIC	2	/* dev. specific stuff valid flag */
	int	emu_type;		/* emulation type (-1 if unused) */
	int	dev_specific[FB_ATTR_NDEVSPECIFIC];	/* catchall */
} fbsattr;


typedef struct fbgattr {
	int	real_type;		/* real device type */
	int	owner;			/* PID of owner, 0 if myself */
	struct	fbtype fbtype;		/* fbtype info for real device */
	struct	fbsattr sattr;		/* see above */
	int	emu_types[FB_ATTR_NEMUTYPES];	/* possible emulations */
						/* (-1 if unused) */
} fbgattr;

struct	fbpixrect {
    struct pixrect	*fbpr_pixrect;	/* Pixrect of dev returned here. */
};

#endif !ASM

#endif /* _FBDEV */
