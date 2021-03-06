#
# Prototype Makefile for machine-dependent directories.
#
# A file of this form resides in each ".md" subdirectory of a
# command.  Its name is typically "md.mk".  During makes in the
# parent directory, this file (or a similar file in a sibling
# subdirectory) is included to define machine-specific things
# such as additional source and object files.
#
# This Makefile is automatically generated.
# DO NOT EDIT IT OR YOU MAY LOSE YOUR CHANGES.
#
# Generated from /sprite/lib/mkmf/Makefile.md
# Sat Sep 22 22:53:30 PDT 1990
#
# For more information, refer to the mkmf manual page.
#
# $Header: /sprite/lib/mkmf/RCS/Makefile.md,v 1.6 90/03/12 23:28:42 jhh Exp $
#
# Allow mkmf

SRCS		= StdioFileCloseProc.c StdioFileOpenMode.c StdioFileReadProc.c StdioFileWriteProc.c Stdio_Setup.c _cleanup.c clearerr.c fclose.c fdopen.c fflush.c fgetc.c fgets.c fileno.c fopen.c fprintf.c fputc.c fputs.c fread.c freopen.c fscanf.c fseek.c ftell.c fwrite.c getchar.c gets.c getw.c perror.c printf.c putchar.c puts.c putw.c rewind.c scanf.c setbuf.c setbuffer.c setlinebuf.c setvbuf.c sprintf.c sscanf.c tmpnam.c ungetc.c vfprintf.c vfscanf.c vprintf.c vsnprintf.c vsprintf.c
HDRS		= fileInt.h
MDPUBHDRS	= 
OBJS		= ds3100.md/StdioFileCloseProc.o ds3100.md/StdioFileOpenMode.o ds3100.md/StdioFileReadProc.o ds3100.md/StdioFileWriteProc.o ds3100.md/Stdio_Setup.o ds3100.md/_cleanup.o ds3100.md/clearerr.o ds3100.md/fclose.o ds3100.md/fdopen.o ds3100.md/fflush.o ds3100.md/fgetc.o ds3100.md/fgets.o ds3100.md/fileno.o ds3100.md/fopen.o ds3100.md/fprintf.o ds3100.md/fputc.o ds3100.md/fputs.o ds3100.md/fread.o ds3100.md/freopen.o ds3100.md/fscanf.o ds3100.md/fseek.o ds3100.md/ftell.o ds3100.md/fwrite.o ds3100.md/getchar.o ds3100.md/gets.o ds3100.md/getw.o ds3100.md/perror.o ds3100.md/printf.o ds3100.md/putchar.o ds3100.md/puts.o ds3100.md/putw.o ds3100.md/rewind.o ds3100.md/scanf.o ds3100.md/setbuf.o ds3100.md/setbuffer.o ds3100.md/setlinebuf.o ds3100.md/setvbuf.o ds3100.md/sprintf.o ds3100.md/sscanf.o ds3100.md/tmpnam.o ds3100.md/ungetc.o ds3100.md/vfprintf.o ds3100.md/vfscanf.o ds3100.md/vprintf.o ds3100.md/vsprintf.o ds3100.md/vsnprintf.o
CLEANOBJS	= ds3100.md/StdioFileCloseProc.o ds3100.md/StdioFileOpenMode.o ds3100.md/StdioFileReadProc.o ds3100.md/StdioFileWriteProc.o ds3100.md/Stdio_Setup.o ds3100.md/_cleanup.o ds3100.md/clearerr.o ds3100.md/fclose.o ds3100.md/fdopen.o ds3100.md/fflush.o ds3100.md/fgetc.o ds3100.md/fgets.o ds3100.md/fileno.o ds3100.md/fopen.o ds3100.md/fprintf.o ds3100.md/fputc.o ds3100.md/fputs.o ds3100.md/fread.o ds3100.md/freopen.o ds3100.md/fscanf.o ds3100.md/fseek.o ds3100.md/ftell.o ds3100.md/fwrite.o ds3100.md/getchar.o ds3100.md/gets.o ds3100.md/getw.o ds3100.md/perror.o ds3100.md/printf.o ds3100.md/putchar.o ds3100.md/puts.o ds3100.md/putw.o ds3100.md/rewind.o ds3100.md/scanf.o ds3100.md/setbuf.o ds3100.md/setbuffer.o ds3100.md/setlinebuf.o ds3100.md/setvbuf.o ds3100.md/sprintf.o ds3100.md/sscanf.o ds3100.md/tmpnam.o ds3100.md/ungetc.o ds3100.md/vfprintf.o ds3100.md/vfscanf.o ds3100.md/vprintf.o ds3100.md/vsnprintf.o ds3100.md/vsprintf.o
INSTFILES	= ds3100.md/md.mk ds3100.md/dependencies.mk Makefile tags TAGS
SACREDOBJS	= 
