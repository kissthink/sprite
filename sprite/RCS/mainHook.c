/* 
 * mainHook.c --
 *
 *	Definitions to modify the behavior of the main routine.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header$ SPRITE (Berkeley)";
#endif not lint

#include "sprite.h"

/*
 * Flags to modify main's behavior.   Can be changed without recompiling
 * by using adb to modify the binary.
 */

Boolean main_Debug 	= FALSE; /* If TRUE then enter the debugger */
Boolean main_DoProf 	= FALSE; /* If TRUE then start profiling */
Boolean main_DoDumpInit	= TRUE; /* If TRUE then initialize dump routines */
int main_NumRpcServers	= 2;	 /* # of rpc servers to create */
char *main_AltInit	= NULL;  /* If non-null then contains name of
				  * alternate init program to use. */
Boolean main_AllowNMI = FALSE;	 /* TRUE -> allow non-maskable intrrupts */


/*
 *----------------------------------------------------------------------
 *
 * Main_HookRoutine --
 *
 *	A routine called by main() just before the init program
 *	is started.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */

void
Main_HookRoutine()
{
}


/*
 *----------------------------------------------------------------------
 *
 * Main_InitVars --
 *
 *	A routine called by main() before it does anything.  Can only be used
 *	to initialize variables and nothing else.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */
void
Main_InitVars()
{
}

