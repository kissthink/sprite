
/*
 * initsprite --
 *
 *	This program is the first user process execed by the Sprite kernel.
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
static char rcsid[] = "$Header: /user5/kupfer/spriteserver/src/cmds/initsprite/RCS/initsprite.c,v 1.3 92/04/29 22:15:18 kupfer Exp $ SPRITE (Berkeley)";
#endif not lint

#include "option.h"
#include <sprite.h>
#include <errno.h>
#include <fs.h>
#include <fsCmd.h>
#include <host.h>
#include <signal.h>
#include <status.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysStats.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <test.h>

/*
 * Initial input/output device to open.
 */
#define CONSOLE		"/dev/console"

/*
 * Command script to execute after doing bare-bones initialization.
 * If a host-dependent script exists, it is used in preference to
 * this one.
 */
#define BOOTCMDS	"/boot/bootcmds.sprited"

/*
 * Program to exec if we're coming up single user.
 */
char	login[]	=		"login";

/*
 * Name of temporary prefix for root partition.
 */
char	prefix[] = 		"/";

/*
 * Name of local boot directory. If we have one we are booting
 * standalone.
 */
char	localBootDir[] = 	"/boot";


/*
 *  Command that was used to boot the machine.
 */
char			*bootCommand = NULL;

/*
 * TRUE => run a login after initial setup.
 */
Boolean			singleUser = FALSE;

/*
 * TRUE => check any disks
 */
Boolean			checkDisk = TRUE;

int		dummy;

Option	optionArray[] = {
    {OPT_STRING, "b", (Address) &bootCommand, "Boot string passed to prom."},
    {OPT_TRUE, "s", (Address) &singleUser, "Boot single-user mode."},
    {OPT_FALSE, "f", (Address) &checkDisk, "Don't check disks (fastboot)."},
    {OPT_TRUE, "root", (Address) &dummy, "Used by kernel, but not initsprite."},
    {OPT_TRUE, "nonroot", (Address) &dummy, 
	    "Used by kernel, but not initsprite."},
    {OPT_STRING, "rootdisk", (Address) &dummy, 
	    "Used by kernel, but not initsprite."},
};
int numOptions = sizeof(optionArray) / sizeof(Option);

/*
 *----------------------------------------------------------------------
 *
 * Exec --
 *
 *	Utility procedure to run a command and wait for the child
 *	to exit.
 *
 * Results:
 *	Returns 0 if all went well, -1 otherwise.
 *
 * Side effects:
 *	Prints a message if an error occurs.
 *
 *----------------------------------------------------------------------
 */

int
Exec(name, argv)
    char *name;			/* Name of file to exec. */
    char **argv;		/* Null-terminated array of arguments. */
{
    int pid, childPid;
    union wait status;

    pid = fork();
    if (pid == 0) {
	execvp(name, argv);
	_exit(1);
    }
    if (pid < 0) {
	fprintf(stderr,
		"Initsprite couldn't fork child for \"%s\": %s\n",
		name, strerror(errno));
	return -1;
    }
    childPid = wait(&status);
    if (childPid < 0) {
	fprintf(stderr,
		"Initsprite couldn't wait for \"%s\": %s\n",
		name, strerror(errno));
	return -1;
    }
    if (childPid != pid) {
	fprintf(stderr, "Initsprite waited for child 0x%x, got child 0x%x.\n",
		pid, childPid);
	return -1;
    }
    if (WIFSTOPPED(status)) {
	fprintf(stderr, "Initsprite child \"%s\" stopped instead of exiting.\n",
		name);
	return -1;
    }
    if (status.w_retcode != 0) {
	return -1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main program for first user process in Sprite.
 *
 * Results:
 *	Never returns.
 *
 * Side effects:
 *	Lots.  Read the code..
 *
 *----------------------------------------------------------------------
 */

main(argc, argv)
    int		argc; 		/* Number of arguments */
    char	**argv;		/* Arguments */
{
    char			*argArray[20];
    static struct sigvec	action = {SIG_IGN, 0, 0};
    static char			string[200];
#define HOST_NAME_LENGTH 255
    static char			hostName[HOST_NAME_LENGTH];
    static char			hostID[10];
    int				i;
    int				status;
    int				argsReturned;
    static char			hostMachType[50];
    char			pathname[MAXPATHLEN];
    Host_Entry 			*hostInfo;
    Fs_Prefix			prefix;
    Host_Entry 			*rootInfo;



    argsReturned = Opt_Parse(argc, argv, optionArray, numOptions, 0);

    /*
     * Try to access the local boot directory. If it exists, then
     * we have a local disk and we have to look for everything relative to
     * the prefix. Otherwise we look relative to /.
     */
    /*
     * Open the console which will in turn get inherited as stdin etc.
     * by the shells that are forked.  After this, stdio can be used
     * for I/O.  If this fails, exit with status 2 to indicate what
     * happened (can't print a message, since there's no I/O yet).
     */

    if ((open(CONSOLE, O_RDONLY, 0) != 0)
	    || (open(CONSOLE, O_WRONLY, 0) != 1)
	    || (open(CONSOLE, O_WRONLY, 0) != 2)) {
	Test_PutMessage("Initsprite couldn't open console ");
	Test_PutMessage(CONSOLE);
	Test_PutMessage(": ");
	Test_PutMessage(strerror(errno));
	Test_PutMessage("\n");
	exit(2);
    }
    if (getwd(pathname) == 0) {
	printf("Couldn't get working directory: %s\n", pathname);
    } else {
	printf("%s/initsprite starting up.\n", pathname);
    }
    if (argsReturned > 1) {
	Opt_PrintUsage(argv[0], optionArray, Opt_Number(optionArray));
    }
    /*
     * Set up signal actions.
     */

    if (sigvec(SIGINT, &action, (struct sigvec *) NULL) != 0) {
	fprintf(stderr,
		"Warning: initsprite can't ignore SIGINT signals: %s\n", 
		strerror(errno));
    }
    if (sigvec(SIGQUIT, &action, (struct sigvec *) NULL) != 0) {
	fprintf(stderr,
		"Warning: initsprite can't ignore SIGQUIT signals: %s\n",
		strerror(errno));
    }
    if (sigvec(SIGHUP, &action, (struct sigvec *) NULL) != 0) {
	fprintf(stderr,
		"Warning: initsprite can't ignore SIGHUP signals: %s\n",
		strerror(errno));
    }
    if (sigvec(SIGTERM, &action, (struct sigvec *) NULL) != 0) {
	fprintf(stderr,
		"Warning: initsprite can't ignore SIGTERM signals: %s\n",
		strerror(errno));
    }
    /*
     * Set up the environment.
     */

    if (gethostname(hostName, HOST_NAME_LENGTH) != 0) {
	fprintf(stderr, "Initsprite couldn't get host name: %s\n",
		strerror(errno));
	goto exitError;
    }
    hostName[HOST_NAME_LENGTH-1] = 0;
    printf("hostName is %s.\n", hostName);
    hostInfo = Host_ByName(hostName);
    if (hostInfo == NULL) {
	fprintf(stderr,
		"Initsprite can't get information about host \"%s\": %s\n",
		hostName, strerror(errno));
	goto exitError;
    }
    strncpy(hostMachType, hostInfo->machType, 50);
    sprintf(hostID, "%d", hostInfo->id);
    Host_End();
    setenv("HOST", hostName);
    setenv("MACHINE", hostMachType);
    sprintf(string, ".:./cmds:/sprite/cmds");
    setenv("PATH", string);
    setenv("HOME", "/");
    setenv("USER", "root");

    if (singleUser) {
	/*
	 *	If we are coming up single user then go to a login.
	 */
	fprintf(stderr,"Single user mode requires root password.\n");
	argArray[0] = "login";
	argArray[1] = "-d";
	argArray[2] = CONSOLE;
	argArray[3] = "-l";
	argArray[4] = "root";
	argArray[5] = "-t";
	argArray[6] = 0;
	status = Exec(login, argArray);
	if (status != 0) {
	    argArray[0] = "csh";
	    argArray[1] = "-i";
	    argArray[2] = 0;
	    status = Exec("csh", argArray);
	}
	if (status != 0) {
	    fprintf(stderr, "Can't run a shell.\n");
	}
	fprintf(stderr,"Initsprite continuing.\n");
    }
    /*
     * Find the root server.
     */
    status = SUCCESS;
    printf("Searching for root prefix.\n");
    for (i = 0; status == SUCCESS; i++) {
	bzero((char *) &prefix, sizeof(Fs_Prefix));
	status = Sys_Stats(SYS_FS_PREFIX_STATS, i, (Address) &prefix);
	if (status == SUCCESS) {
	    if (!strcmp(prefix.prefix, "/")) {
		rootInfo = Host_ByID(prefix.serverID);
		printf("Setting ROOTSERVER = %s\n", rootInfo->name);
		setenv("ROOTSERVER", rootInfo->name);
		break;
	    }
	}
    }
    /*
     * If there is a bootcmds file in the host-specific directory, execute
     * it instead of the one in /.  Remember that the /.cshrc file will
     * also be loaded by csh when processing bootcmds.
     */
    
    sprintf(string, "/hosts/%.50s/bootcmds.sprited", hostName);
    if (access(string, R_OK) != 0) {
	strcpy(string, BOOTCMDS);
    }
    fprintf(stderr, "Executing %s\n", string);
    i = 0;
    argArray[i++] = "csh";
    argArray[i++] = string;
    if (bootCommand != NULL) {
	argArray[i++] = "-b";
	argArray[i++] = bootCommand;
    }
    if (!checkDisk) {
	argArray[i++] = "-f";
    }
    argArray[i++] = "-i";
    argArray[i++] = hostID;
    argArray[i++] = 0;
    if (Exec("csh", argArray) == 0) {
	exit(0);
    }
    /*
     * The boot command script did not complete successfully.  
     * Try to bail out to a shell.
     */

    fprintf(stderr, "Initsprite: boot command file exited abnormally!\n");

    argArray[0] = "csh";
    argArray[1] = "-i";
    argArray[2] = 0;
    execvp("csh", argArray);
    fprintf(stderr, "Init failed when trying to bailout to csh: %s\n",
	    strerror(errno));
    fprintf(stderr, "Init has run out of things to try:  nothing works.\n");
exitError:
    fprintf(stderr, "Initsprite exiting.\n");
    exit(1);
}
