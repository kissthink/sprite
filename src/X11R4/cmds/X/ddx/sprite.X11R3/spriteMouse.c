/*-
 * spriteMouse.c --
 *	Functions for playing cat and mouse... sorry.
 *
 * Copyright (c) 1987, 1989 by the Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 */
#ifndef lint
static char rcsid[] =
	"$Header: /a/X/src/cmds/Xsprite/ddx/sun3.md/RCS/spriteMouse.c,v 1.2 87/06/20 19:56:48 deboor Exp Locker: ouster $ SPRITE (Berkeley)";
#endif lint

#define NEED_EVENTS
#include    "spriteddx.h"

typedef struct {
    int	    bmask;	    /* Current button state */
    Bool    mouseMoved;	    /* Mouse has moved */
} SunMsPrivRec, *SunMsPrivPtr;

static void 	  	spriteMouseCtrl();
static int 	  	spriteMouseGetMotionEvents();
static void 	  	spriteMouseProcessEvent();
static void 	  	spriteMouseDoneEvents();

/*
 * Because lastEventTime is actual time, while event times are since-boot
 * times, we keep the time of the most recent mouse event for
 * the DoneEvents procedure to use.
 */
static int  	  	lastMouseEventTime = 0;

static SunMsPrivRec	spriteMousePriv;
PtrPrivRec 	sysMousePriv = {
    -1,				/* Descriptor to device */
    (Mouse_Event *(*)())NoopDDA,/* Function to read events -- done by
				 * keyboard */
    spriteMouseProcessEvent,	/* Function to process an event */
    spriteMouseDoneEvents,	/* When all the events have been */
				/* handled, this function will be */
				/* called. */
    0,				/* Current X coordinate of pointer */
    0,				/* Current Y coordinate */
    NULL,			/* Screen pointer is on */
    (pointer)&spriteMousePriv,	/* Field private to device */
};

/*-
 *-----------------------------------------------------------------------
 * spriteMouseProc --
 *	Handle the initialization, etc. of a mouse
 *
 * Results:
 *	none.
 *
 * Side Effects:
 *
 *-----------------------------------------------------------------------
 */
int
spriteMouseProc (pMouse, what)
    DevicePtr	  pMouse;   	/* Mouse to play with */
    int	    	  what;	    	/* What to do with it */
{
    BYTE    	  map[4];

    switch (what) {
	case DEVICE_INIT:
	    if (pMouse != LookupPointerDevice()) {
		ErrorF ("Cannot open non-system mouse");	
		return (!Success);
	    }
	    
	    sysMousePriv.pScreen = &screenInfo.screen[0];
	    sysMousePriv.x = sysMousePriv.pScreen->width / 2;
	    sysMousePriv.y = sysMousePriv.pScreen->height / 2;

	    spriteMousePriv.bmask = 0x87;   /* All buttons up */
	    spriteMousePriv.mouseMoved = FALSE;

	    pMouse->devicePrivate = (pointer) &sysMousePriv;
	    pMouse->on = FALSE;
	    map[1] = 1;
	    map[2] = 2;
	    map[3] = 3;
	    InitPointerDeviceStruct(
		pMouse, map, 3, spriteMouseGetMotionEvents, spriteMouseCtrl);
	    break;
	case DEVICE_ON:
	    break;
	case DEVICE_CLOSE:
	case DEVICE_OFF:
	    break;
    }
    return (Success);
}
	    
/*-
 *-----------------------------------------------------------------------
 * spriteMouseCtrl --
 *	Alter the control parameters for the mouse. Since acceleration
 *	etc. is done from the PtrCtrl record in the mouse's device record,
 *	there's nothing to do here.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static void
spriteMouseCtrl (pMouse)
    DevicePtr	  pMouse;
{
}

/*-
 *-----------------------------------------------------------------------
 * spriteMouseGetMotionEvents --
 *	Return the (number of) motion events in the "motion history
 *	buffer" (snicker) between the given times.
 *
 * Results:
 *	The number of events stuffed.
 *
 * Side Effects:
 *	The relevant xTimecoord's are stuffed in the passed memory.
 *
 *-----------------------------------------------------------------------
 */
static int
spriteMouseGetMotionEvents (buff, start, stop)
    CARD32 start, stop;
    xTimecoord *buff;
{
    return 0;
}

/*-
 *-----------------------------------------------------------------------
 * MouseAccelerate --
 *	Given a delta and a mouse, return the acceleration of the delta.
 *
 * Results:
 *	The corrected delta
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static short
MouseAccelerate (pMouse, delta)
    DevicePtr	  pMouse;
    int	    	  delta;
{
    register int  sgn = sign(delta);
    register PtrCtrl *pCtrl;

    delta = abs(delta);
    pCtrl = &((DeviceIntPtr) pMouse)->u.ptr.ctrl;

    if (delta > pCtrl->threshold) {
	return ((short) (sgn * (pCtrl->threshold +
				((delta - pCtrl->threshold) * pCtrl->num) /
				pCtrl->den)));
    } else {
	return ((short) (sgn * delta));
    }
}

/*-
 *-----------------------------------------------------------------------
 * spriteMouseProcessEvent --
 *	Given a Firm_event for a mouse, pass it off the the dix layer
 *	properly converted...
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The cursor may be redrawn...? devPrivate/x/y will be altered.
 *
 *-----------------------------------------------------------------------
 */
static void
spriteMouseProcessEvent (pMouse, ev)
    DevicePtr	  pMouse;   	/* Mouse from which the event came */
    Mouse_Event  *ev;	    	/* Event to process */
{
    xEvent		xE;
    register PtrPrivPtr	pPriv;	/* Private data for pointer */
    register SunMsPrivPtr pSunPriv; /* Private data for mouse */
    register int  	bmask;	/* Temporary button mask */
    register int  	button;

    pPriv = (PtrPrivPtr)pMouse->devicePrivate;
    pSunPriv = (SunMsPrivPtr) pPriv->devPrivate;

    xE.u.keyButtonPointer.time = ev->time;

    bmask = ev->key ^ pSunPriv->bmask;

    if (ev->deltaX) {
	pPriv->x += MouseAccelerate (pMouse, ev->deltaX);
	if (spriteConstrainXY (&pPriv->x, &pPriv->y)) {
	    pSunPriv->mouseMoved = TRUE;
	}
    }

    if (ev->deltaY) {
	pPriv->y -= MouseAccelerate (pMouse, ev->deltaY);
	if (spriteConstrainXY (&pPriv->x, &pPriv->y)) {
	    pSunPriv->mouseMoved = TRUE;
	}
    }
    
    if (ev->key ^ pSunPriv->bmask) {
	spriteMouseDoneEvents (pMouse, FALSE);
    }
    xE.u.keyButtonPointer.rootX = pPriv->x;
    xE.u.keyButtonPointer.rootY = pPriv->y;
    xE.u.keyButtonPointer.time = ev->time;

    for (bmask = 4, button = 1; bmask != 0; bmask >>= 1, button++) {
	if ((ev->key & bmask) != (pSunPriv->bmask & bmask)) {
	    xE.u.u.type = (ev->key & bmask) ? ButtonRelease : ButtonPress;
	    xE.u.u.detail = button;

	    (* pMouse->processInputProc) (&xE, pMouse);
	}
    }
    pSunPriv->bmask = ev->key;
    lastMouseEventTime = ev->time;
}

/*-
 *-----------------------------------------------------------------------
 * spriteMouseDoneEvents --
 *	Finish off any mouse motions we haven't done yet. (At the moment
 *	this code is unused since we never save mouse motions as I'm
 *	unsure of the effect of getting a keystroke at a given [x,y] w/o
 *	having gotten a motion event to that [x,y])
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A MotionNotify event may be generated.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
spriteMouseDoneEvents (pMouse, final)
    DevicePtr	  pMouse;
    Bool	  final;
{
    PtrPrivPtr	  pPriv;
    SunMsPrivPtr  pSunPriv;
    xEvent	  xE;

    pPriv = (PtrPrivPtr) pMouse->devicePrivate;
    pSunPriv = (SunMsPrivPtr) pPriv->devPrivate;

    if (pSunPriv->mouseMoved) {
	spriteMoveCursor (pPriv->pScreen, pPriv->x, pPriv->y);
	xE.u.keyButtonPointer.rootX = pPriv->x;
	xE.u.keyButtonPointer.rootY = pPriv->y;
	xE.u.keyButtonPointer.time = lastMouseEventTime;
	xE.u.u.type = MotionNotify;
	(* pMouse->processInputProc) (&xE, pMouse);
	pSunPriv->mouseMoved = FALSE;
    }
}
