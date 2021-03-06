#include "sprite.h"
#include "sys/file.h"
#include "errno.h"
#include "stdio.h"
#include "dev/graphics.h"
#include "sys/ioctl.h"

char	colorServer[] = "/X11/R4/cmds/Xcfbpmax";
char	bwServer[] = "/X11/R4/cmds/Xmfbpmax";

main(argc, argv)
    int		argc;
    char	*argv[];
{
    int		fdPM;
    int		isColor;
    char	**newargv;
    int		i;

    /*
     * Determine the server type.
     */
    if ((fdPM = open("/dev/mouse", O_RDWR | O_NDELAY, 0)) < 0) {
	fprintf(stderr, "couldn't open /dev/mouse \n");
	return FALSE;
    }
    if (ioctl(fdPM, QIOISCOLOR, &isColor) < 0) {
	extern int errno;
	int en = errno;

	fprintf(stderr, "errno = %d, ", en);
	fprintf(stderr, "error doing ioctl\n");
	close(fdPM);
	return FALSE;
    }
    close(fdPM);

    /*
     * Copy the arguments.
     */
    newargv = (char **) malloc((argc + 1) * sizeof (char *));
    if (isColor) {
	newargv[0] = (char *) malloc(strlen(colorServer) + 1);
	strcpy(newargv[0], colorServer);
    } else {
	newargv[0] = (char *) malloc(strlen(bwServer) + 1);
	strcpy(newargv[0], bwServer);
    }

    for (i = 1; i < argc ; i++) {
	newargv[i] = (char *) malloc(strlen(argv[i]) + 1);
	strcpy(newargv[i], argv[i]);
    }
    newargv[i] = NULL;

    /*
     * Exec the server.
     */
    execvp(newargv[0], newargv);

    fprintf(stderr, "Unable to exec %s\n", newargv[0]);

    exit(1);
}
