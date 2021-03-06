/* 
 * ioctl.c --
 *
 *	Procedure to map from Unix ioctl system call to Sprite.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/lib/c/emulator/RCS/ioctl.c,v 1.3 92/03/23 15:00:48 kupfer Exp $ SPRITE (Berkeley)";
#endif not lint

#include <cfuncproto.h>

#include <sys/fb.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sprite.h>
#include <stdio.h>
#include <compatInt.h>
#include <dev/tty.h>
#include <dev/net.h>
#include <dev/graphics.h>
#include <fs.h>
#include <errno.h>

#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ttychars.h>
#include <sys/ttydev.h>
#include <net/route.h>
#include <net/if.h>
#include <sys/time.h>
#include <dev/audio.h>
#include <sys/audioio.h>

static void DecodeRequest _ARGS_((int request));

#ifdef notdef
int compatTapeIOCMap[] = {
    IOC_TAPE_WEOF, 		/* 0, MTWEOF */
    IOC_TAPE_SKIP_FILES,	/* 1, MTFSF */
    IOC_TAPE_BACKUP_FILES,	/* 2, MTBSF */
    IOC_TAPE_SKIP_BLOCKS,	/* 3, MTFSR */
    IOC_TAPE_BACKUP_BLOCKS,	/* 4, MTBSR */
    IOC_TAPE_REWIND, 		/* 5, MTREW */
    IOC_TAPE_OFFLINE, 		/* 6, MTOFFL */
    IOC_TAPE_NO_OP,		/* 7, MTNOP */
    IOC_TAPE_RETENSION,		/* 8, MTRETEN */
    IOC_TAPE_ERASE,		/* 9, MTERASE */
};
#endif


/*
 *----------------------------------------------------------------------
 *
 * ioctl --
 *
 *	Compat procedure for Unix ioctl call.
 *
 * Results:
 *	Returns UNIX_ERROR if there's no known way to emulate the ioctl
 *	under Sprite.  Otherwise, returns whatever UNIX would return.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ioctl(fd, request, buf)
    int		fd;
    int		request;
    char *	buf;
{
    ReturnStatus	status;

    switch (request) {

	/*
	 * Generic calls:
	 */

	case FIOCLEX: {
		/*
		 * Set the close-on-exec bit for the file.
		 * Buf is not used.
		 */

		int flag = IOC_CLOSE_ON_EXEC;
		status = Fs_IOControl(fd, IOC_SET_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
	    }
	    break;

	case FIONCLEX: {
		/*
		 * Clear the close-on-exec bit for the file.
		 * Buf is not used.
		 */

		int flag = IOC_CLOSE_ON_EXEC;
		status = Fs_IOControl(fd, IOC_CLEAR_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
	    }
	    break;

	case FIONREAD:
	    status = Fs_IOControl(fd, IOC_NUM_READABLE, 0, (Address) NULL,
			sizeof(int), buf);
	    break;

        case FIONBIO: {
		/*
		 * Set or clear the non-blocking I/O bit for the file.
		 */

		int flag = IOC_NON_BLOCKING;
		if (*(int *)buf) {
		    status = Fs_IOControl(fd, IOC_SET_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
		} else {
		    status = Fs_IOControl(fd, IOC_CLEAR_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
		}
	    }
	    break;

	case FIOASYNC: {
		/*
		 * Set or clear the asynchronous I/O bit for the file.
		 */

		int flag = IOC_ASYNCHRONOUS;
		if (*(int *)buf) {
		    status = Fs_IOControl(fd, IOC_SET_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
		} else {
		    status = Fs_IOControl(fd, IOC_CLEAR_BITS, 
			    sizeof(flag), (Address) &flag, 0, (Address) NULL);
		}
	    }
	    break;


	case FIOGETOWN:
	case SIOCGPGRP:
	case TIOCGPGRP: {
		int procOrFamily;

		status = Ioc_GetOwner(fd, (int *)buf, &procOrFamily);
	    }
	    break;

	case FIOSETOWN:
	case SIOCSPGRP:
	case TIOCSPGRP:
	    status = Ioc_SetOwner(fd, *(int *)buf, IOC_OWNER_FAMILY);
	    break;

	/* 
	 * Tty-related calls:
	 */

	case TIOCGETP:
	    status = Fs_IOControl(fd, IOC_TTY_GET_PARAMS, 0, (Address) NULL,
		    sizeof(struct sgttyb), (Address) buf);
	    break;
	case TIOCSETP:
	    status = Fs_IOControl(fd, IOC_TTY_SET_PARAMS, sizeof(struct sgttyb),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCSETN:
	    status = Fs_IOControl(fd, IOC_TTY_SETN, sizeof(struct sgttyb),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCEXCL:
	    status = Fs_IOControl(fd, IOC_TTY_EXCL, 0, (Address) NULL,
		    0, (Address) NULL);
	    break;
	case TIOCNXCL:
	    status = Fs_IOControl(fd, IOC_TTY_NXCL, 0, (Address) NULL,
		    0, (Address) NULL);
	    break;
	case TIOCHPCL:
	    status = Fs_IOControl(fd, IOC_TTY_HUP_ON_CLOSE, 0,
		    (Address) NULL, 0, (Address) NULL);
	    break;
	case TIOCFLUSH:
	    status = Fs_IOControl(fd, IOC_TTY_FLUSH, sizeof(int),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCSTI:
	    status = Fs_IOControl(fd, IOC_TTY_INSERT_CHAR, sizeof(char),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCSBRK:
	    status = Fs_IOControl(fd, IOC_TTY_SET_BREAK, 0,
		    (Address) NULL, 0, (Address) NULL);
	    break;
	case TIOCCBRK:
	    status = Fs_IOControl(fd, IOC_TTY_CLEAR_BREAK, 0,
		    (Address) NULL, 0, (Address) NULL);
	    break;
	case TIOCSDTR:
	    status = Fs_IOControl(fd, IOC_TTY_SET_DTR, 0, 
		    (Address) NULL, 0, (Address) NULL);
	    break;
	case TIOCCDTR:
	    status = Fs_IOControl(fd, IOC_TTY_CLEAR_DTR, 0,
		    (Address) NULL, 0, (Address) NULL);
	    break;
	case TIOCGETC:
	    status = Fs_IOControl(fd, IOC_TTY_GET_TCHARS, 0, (Address) NULL,
		    sizeof(struct tchars), (Address) buf);
	    break;
	case TIOCSETC:
	    status = Fs_IOControl(fd, IOC_TTY_SET_TCHARS,
		    sizeof(struct tchars), (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCGLTC:
	    status = Fs_IOControl(fd, IOC_TTY_GET_LTCHARS, 0, (Address) NULL,
		    sizeof(struct ltchars), (Address) buf);
	    break;
	case TIOCSLTC:
	    status = Fs_IOControl(fd, IOC_TTY_SET_LTCHARS,
		    sizeof(struct ltchars), (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCLBIS:
	    status = Fs_IOControl(fd, IOC_TTY_BIS_LM,
		    sizeof(int), (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCLBIC:
	    status = Fs_IOControl(fd, IOC_TTY_BIC_LM,
		    sizeof(int), (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCLSET:
	    status = Fs_IOControl(fd, IOC_TTY_SET_LM,
		    sizeof(int), (Address) buf, 0, (Address) NULL);
	    break;
	case TIOCLGET:
	    status = Fs_IOControl(fd, IOC_TTY_GET_LM, 0, (Address) NULL,
		    sizeof(int), (Address) buf);
	    break;
	case TIOCGETD:
	    status = Fs_IOControl(fd, IOC_TTY_GET_DISCIPLINE, 0,
		    (Address) NULL, sizeof(int), (Address) buf);
	    break;
	case TIOCSETD:
	    status = Fs_IOControl(fd, IOC_TTY_SET_DISCIPLINE,
		sizeof(int), (Address) buf, 0, (Address) NULL);
	    break;

	case SIOCATMARK:
	    status = Fs_IOControl(fd, IOC_NET_IS_OOB_DATA_NEXT,
			    0, (Address) NULL, sizeof(int), (Address) buf);
	    break;

        case TCGETS:
	    status = Fs_IOControl(fd, IOC_TTY_GET_TERMIO,
		0, (Address) NULL, sizeof(struct termios), (Address) buf);
	    break;

        case TCSETS:
	    status = Fs_IOControl(fd, IOC_TTY_SET_TERMIO,
		sizeof(struct termios), (Address) buf, 0, (Address) NULL);
	    break;

	case TIOCGWINSZ:
	    status = Fs_IOControl(fd, IOC_TTY_GET_WINDOW_SIZE,
		0, (Address) NULL, sizeof(struct winsize), (Address) buf);
	    break;

        case TIOCSWINSZ:
	    status = Fs_IOControl(fd, IOC_TTY_SET_WINDOW_SIZE,
		sizeof(struct winsize), (Address) buf, 0, (Address) NULL);
	    break;

        case TIOCNOTTY:
	    status = Fs_IOControl(fd, IOC_TTY_NOT_CONTROL_TTY, 0,
		    (Address) NULL, 0, (Address) NULL);
	    break;
	/*
	 * Graphics requests - decstations.
	 */
	case QIOCGINFO:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_GET_INFO,
		0, (Address) NULL, sizeof(DevScreenInfo *), (Address)buf);
	    break;
	case QIOCPMSTATE:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_MOUSE_POS,
		sizeof(DevCursor), (Address) buf, 0, (Address)NULL);
	    break;
	case QIOWCURSORCOLOR:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_CURSOR_COLOR,
		sizeof(unsigned int [6]), (Address) buf, 0, (Address)NULL);
	    break;
	case QIOCINIT:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_INIT_SCREEN,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case QIOCKPCMD:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_KBD_CMD,
		sizeof(DevKpCmd), (Address) buf, 0, (Address)NULL);
	    break;
	case QIOCADDR:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_GET_INFO_ADDR,
		0, (Address) NULL, sizeof(DevScreenInfo *), (Address)buf);
	    break;
	case QIOWCURSOR:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_CURSOR_BIT_MAP,
		sizeof(short[32]), (Address) buf, 0, (Address)NULL);
	    break;
	case QIOKERNLOOP:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_KERN_LOOP,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case QIOKERNUNLOOP:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_KERN_UNLOOP,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case QIOVIDEOON:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_VIDEO_ON,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case QIOVIDEOOFF:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_VIDEO_OFF,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case QIOSETCMAP:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_COLOR_MAP,
		sizeof(DevColorMap), (Address) buf, 0, (Address)NULL);
	    break;
	case QIOISCOLOR:
	    status = Fs_IOControl(fd, IOC_GRAPHICS_IS_COLOR,
		0, (Address) NULL, sizeof (int), (Address) buf);
	    break;
	case SIOCGIFCONF: {
	    struct ifconf *ifcPtr;

	    /*
	     * Fake this IO control so that X will work (gasp!!).
	     */
	    ifcPtr = (struct ifconf *)buf;
	    ifcPtr->ifc_len = 0;
	    status = SUCCESS;
	    break;
	}
	/*
	 * Graphics requests - suns.
	 */
	case FBIOGTYPE:
	    status = Fs_IOControl(fd, FBIOGTYPE, 0, (Address) NULL,
		    sizeof (fbtype), (Address) buf);
	    break;
	case FBIOPUTCMAP:
	    status = Fs_IOControl(fd, FBIOPUTCMAP, sizeof (fbcmap),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case FBIOSVIDEO:
	    status = Fs_IOControl(fd, FBIOSVIDEO, sizeof (int),
		    (Address) buf, 0, (Address) NULL);
	    break;
	case FBIOGVIDEO:
	    status = Fs_IOControl(fd, FBIOGVIDEO, 0, (Address) NULL,
		    sizeof (int), (Address) buf);
	    break;
	case FBIOGPIXRECT:	/* not implemented yet */
	case FBIOGINFO:		/* not implemented yet */
	case FBIOGETCMAP:	/* not implemented yet */
	case FBIOSATTR:		/* not implemented yet */
	case FBIOGATTR:		/* not implemented yet */
	case FBIOVERTICAL:	/* not implemented yet */
	    errno = EINVAL;
	    return(UNIX_ERROR);
	/*
	 * Audio requests.
	 */
	case AUDIOGETREG:
	    status = Fs_IOControl(fd, IOC_AUDIO_GETREG,
		sizeof(struct audio_ioctl), (Address) buf, sizeof(struct
		audio_ioctl), (Address)buf);
	    break;
	case AUDIOSETREG:
	    status = Fs_IOControl(fd, IOC_AUDIO_SETREG,
		sizeof(DevColorMap), (Address) buf, 0, (Address)NULL);
	    break;
	case AUDIOREADSTART:
	    status = Fs_IOControl(fd, IOC_AUDIO_READSTART,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case AUDIOSTOP:
	    status = Fs_IOControl(fd, IOC_AUDIO_STOP,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case AUDIOPAUSE:
	    status = Fs_IOControl(fd, IOC_AUDIO_PAUSE,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case AUDIORESUME:
	    status = Fs_IOControl(fd, IOC_AUDIO_RESUME,
		0, (Address) NULL, 0, (Address)NULL);
	    break;
	case AUDIOREADQ:
	    status = Fs_IOControl(fd, IOC_AUDIO_READQ,
		0, (Address) NULL, sizeof(int), (Address)buf);
	    break;
	case AUDIOWRITEQ:
	    status = Fs_IOControl(fd, IOC_AUDIO_WRITEQ,
		0, (Address) NULL, sizeof(int), (Address)buf);
	    break;
	case AUDIOGETQSIZE:
	    status = Fs_IOControl(fd, IOC_AUDIO_GETQSIZE,
		0, (Address) NULL, sizeof(int), (Address)buf);
	    break;
	case AUDIOSETQSIZE:
	    status = Fs_IOControl(fd, IOC_AUDIO_SETQSIZE,
		sizeof(int), (Address) buf, 0, (Address)NULL);
	    break;

	/*
	 * Magnetic-tape requests:  need to define dev/mt.h before
	 * re-enabling this code.  Can't use UNIX include files, because
	 * they want ioc.h, which redefines all the tty ioctls.
	 */

#ifdef notdef
	case MTIOCTOP: {
	    /*
	     * Have to peek into the request buffer to map the specific
	     * tape commands.
	     */
	    Dev_TapeCommand cmd;
	    struct mtop *unixCmdPtr = (struct mtop *)buf;

	    if (unixCmdPtr->mt_op > MTERASE ||
		unixCmdPtr->mt_op < 0) {
		errno = EINVAL;
		return(UNIX_ERROR);
	    }
	    cmd.command = compatTapeIOCMap[unixCmdPtr->mt_op];
	    cmd.count = unixCmdPtr->mt_count;
	    status = Ioc_TapeCommand(fd, (Address)&cmd);
	    break;
	}
	case MTIOCGET: {
	    Dev_TapeStatus stat;
	    struct mtget *unixStatPtr;
	    /*
	     * DO ME, check sys/mtio.h
	     */
	}
#endif notdef

	/*
	 * Unknown requests:
	 */

	default:
	    DecodeRequest(request);
	    errno = EINVAL;
	    return(UNIX_ERROR);
    }

    if (status != SUCCESS) {
        errno = Compat_MapCode(status);
        return(UNIX_ERROR);
    }

    return(UNIX_SUCCESS);
}



/*
 *----------------------------------------------------------------------
 *
 * DecodeRequest --
 *
 *	Takes a UNIX ioctl request and prints the name of the request
 *	on the standard error file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

typedef struct {
    char *name;
    int	 request;
} RequestName;

static void
DecodeRequest(request)
    int request;
{
    register int i;
    /*
     * The following list contains all the ioctls in 
     * /usr/include/sys/ioctl.h (SunOS 3.2), and
     * /usr/include/sys/termios.h (SunOS 4.0).
     */
    const static RequestName mapping[] = {
	{"TIOCGETD",	TIOCGETD},
	{"TIOCSETD",	TIOCSETD},
	{"TIOCHPCL",	TIOCHPCL},
	{"TIOCMODG",	TIOCMODG},
	{"TIOCMODS",	TIOCMODS},
	{"TIOCGETP",	TIOCGETP},
	{"TIOCSETP",	TIOCSETP},
	{"TIOCSETN",	TIOCSETN},
	{"TIOCEXCL",	TIOCEXCL},
	{"TIOCNXCL",	TIOCNXCL},
	{"TIOCFLUSH",	TIOCFLUSH},
	{"TIOCSETC",	TIOCSETC},
	{"TIOCGETC",	TIOCGETC},
	{"TIOCLBIS",	TIOCLBIS},
	{"TIOCLBIC",	TIOCLBIC},
	{"TIOCLSET",	TIOCLSET},
	{"TIOCLGET",	TIOCLGET},
	{"TIOCSBRK",	TIOCSBRK},
	{"TIOCCBRK",	TIOCCBRK},
	{"TIOCSDTR",	TIOCSDTR},
	{"TIOCCDTR",	TIOCCDTR},
	{"TIOCSLTC",	TIOCSLTC},
	{"TIOCGLTC",	TIOCGLTC},
	{"TIOCOUTQ",	TIOCOUTQ},
	{"TIOCSTI",	TIOCSTI},
	{"TIOCNOTTY",	TIOCNOTTY},
	{"TIOCPKT",	TIOCPKT},
	{"TIOCSTOP",	TIOCSTOP},
	{"TIOCSTART",	TIOCSTART},
	{"TIOCMSET",	TIOCMSET},
	{"TIOCMBIS",	TIOCMBIS},
	{"TIOCMBIC",	TIOCMBIC},
	{"TIOCMGET",	TIOCMGET},
	{"TIOCREMOTE",	TIOCREMOTE},
	{"TIOCGWINSZ",	TIOCGWINSZ},
	{"TIOCSWINSZ",	TIOCSWINSZ},
	{"TIOCUCNTL",	TIOCUCNTL},
	{"TIOCCONS",	TIOCCONS},
	{"TIOCSSIZE",	TIOCSSIZE},
	{"TIOCGSIZE",	TIOCGSIZE},
	{"SIOCSHIWAT",	SIOCSHIWAT},
	{"SIOCGHIWAT",	SIOCGHIWAT},
	{"SIOCSLOWAT",	SIOCSLOWAT},
	{"SIOCGLOWAT",	SIOCGLOWAT},
	{"SIOCADDRT",	SIOCADDRT},
	{"SIOCDELRT",	SIOCDELRT},
	{"SIOCSIFADDR",	SIOCSIFADDR},
	{"SIOCGIFADDR",	SIOCGIFADDR},
	{"SIOCSIFDSTADDR",	SIOCSIFDSTADDR},
	{"SIOCGIFDSTADDR",	SIOCGIFDSTADDR},
	{"SIOCSIFFLAGS",	SIOCSIFFLAGS},
	{"SIOCGIFFLAGS",	SIOCGIFFLAGS},
	{"SIOCGIFCONF",	SIOCGIFCONF},
	{"SIOCGIFBRDADDR",	SIOCGIFBRDADDR},
	{"SIOCSIFBRDADDR",	SIOCSIFBRDADDR},
	{"SIOCGIFNETMASK",	SIOCGIFNETMASK},
	{"SIOCSIFNETMASK",	SIOCSIFNETMASK},
	{"SIOCGIFMETRIC",	SIOCGIFMETRIC},
	{"SIOCSIFMETRIC",	SIOCSIFMETRIC},
	{"SIOCSARP",	SIOCSARP},
	{"SIOCGARP",	SIOCGARP},
	{"SIOCDARP",	SIOCDARP},
	{"TCXONC",      TCXONC},
	{"TCFLSH",      TCFLSH},
	{"TCGETS",      TCGETS},
	{"TCSETS",      TCSETS},
	{"AUDIOREADSTART",	AUDIOREADSTART},
	{"AUDIOPAUSE",	AUDIOPAUSE},
	{"AUDIORESUME",	AUDIORESUME},
	{"AUDIOSTOP",	AUDIOSTOP},
	{"AUDIOREADQ",	AUDIOREADQ},
	{"AUDIOWRITEQ",	AUDIOWRITEQ},
	{"AUDIOGETQSIZE",	AUDIOGETQSIZE},
	{"AUDIOSETQSIZE",	AUDIOSETQSIZE},
	{"AUDIOSETREG",	AUDIOSETREG},
	{"AUDIOGETREG",	AUDIOGETREG},
    };

    if ((request == TIOCGSIZE) || (request == TIOCGWINSZ)) {
	/*
	 * Special-case TIOCGSIZE since every visually-oriented program
	 * uses it and it's annoying to constantly get the messages.
	 */
	return;
    }

    /*
     * For now search the list linearly. It's slow but simple...
     */
    for (i = sizeof(mapping)/sizeof(*mapping); --i >= 0;) {
	if (request == mapping[i].request) {
	    fprintf(stderr, "ioctl: bad command %s\n", mapping[i].name);
	    return;
	}
    }
    fprintf(stderr, "ioctl: bad command 0x%x\n", request);
    return;
}
