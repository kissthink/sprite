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
# Tue Jul  2 14:16:38 PDT 1991
#
# For more information, refer to the mkmf manual page.
#
# $Header: /sprite/lib/mkmf/RCS/Makefile.md,v 1.6 90/03/12 23:28:42 jhh Exp $
#
# Allow mkmf

SRCS		= ds5000.md/timerMC.c ds5000.md/timerTick.c timerClock.c timerQueue.c timerStubs.c
HDRS		= ds5000.md/timerMach.h ds5000.md/timerTick.h timer.h timerInt.h timerUnixStubs.h
MDPUBHDRS	= ds5000.md/timerMach.h ds5000.md/timerTick.h
OBJS		= ds5000.md/timerClock.o ds5000.md/timerMC.o ds5000.md/timerQueue.o ds5000.md/timerStubs.o ds5000.md/timerTick.o
CLEANOBJS	= ds5000.md/timerMC.o ds5000.md/timerTick.o ds5000.md/timerClock.o ds5000.md/timerQueue.o ds5000.md/timerStubs.o
INSTFILES	= ds5000.md/md.mk ds5000.md/dependencies.mk Makefile local.mk tags TAGS
SACREDOBJS	= 
