/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)inet.h	5.1 (Berkeley) 5/30/85
 */

#ifndef _INET
#define _INET

/*
 * External definitions for
 * functions in inet(3N)
 */
unsigned long inet_addr();
char	*inet_ntoa();
struct	in_addr inet_makeaddr();
unsigned long inet_network();

#endif _INET
