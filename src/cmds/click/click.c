/* 
 * click.c --
 *
 *	This file contains a program that will turn the key click on or off.
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

#ifndef lint
static char rcsid[] = "$Header: /a/newcmds/click/RCS/click.c,v 1.2 89/07/20 10:40:49 ouster Exp $ SPRITE
 (Berkeley)";
#endif not lint

#include <sys/file.h>
#include <stdio.h>

main(argc, argv)
    int		argc;
    char	**argv;
{
    char		buf[100];
    int			i;
    int			fd;

    if (argc != 2) {
	fprintf(stderr, "click [on | off]\n");
	exit(1);
    }
    if (strcmp(argv[1], "on") == 0) {
	buf[0] = 0xa;
    } else if (strcmp(argv[1], "off") == 0) {
	buf[0] = 0xb;
    } else {
	fprintf(stderr, "click [on | off]\n");
	exit(1);
    }

    /*
     * For now (7/20/89) try both /dev/mouse and dev/Xevent, to support
     * both the old and new versions of X.  However, in a few weeks it
     * should be possible to get rid of /dev/Xevent.
     */

    fd = open("/dev/mouse", O_WRONLY, 0);
    if (fd < 0) {
	fd = open("/dev/Xevent", O_WRONLY, 0);
    }
    if (fd < 0) {
	perror("Click: couldn't open /dev/mouse or /dev/Xevent");
	exit(1);
    }
    if (write(fd, buf, 1) != 1) {
	perror("Click: write to /dev/mouse");
	exit(1);
    }
}
