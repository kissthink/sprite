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
# Mon Nov 19 17:46:26 PST 1990
#
# For more information, refer to the mkmf manual page.
#
# $Header: /sprite/lib/mkmf/RCS/Makefile.md,v 1.6 90/03/12 23:28:42 jhh Exp $
#
# Allow mkmf

SRCS		= blockframe.c breakpoint.c command.c dbxread.c environ.c eval.c expprint.c findvar.c infcmd.c inflow.c infrun.c main.c obstack.c printcmd.c remote.c source.c stack.c symmisc.c symtab.c utils.c valarith.c valops.c valprint.c values.c version.c kgdbcmd.c core.c readline.c dep.c pinsn.c expread.y initialized_all_files.c history.c copying.c ptrace.c
HDRS		= command.h defs.h emacs_keymap.c.h environ.h expression.h frame.h funmap.c.h history.h inferior.h keymaps.c.h m-sun3.h obstack.h opcode.h param.h readline.h symseg.h symtab.h value.h vi_keymap.c.h wait.h
MDPUBHDRS	= 
OBJS		= sun4.md/blockframe.o sun4.md/breakpoint.o sun4.md/command.o sun4.md/copying.o sun4.md/core.o sun4.md/dbxread.o sun4.md/dep.o sun4.md/environ.o sun4.md/eval.o sun4.md/expprint.o sun4.md/expread.o sun4.md/findvar.o sun4.md/history.o sun4.md/infcmd.o sun4.md/inflow.o sun4.md/infrun.o sun4.md/initialized_all_files.o sun4.md/kgdbcmd.o sun4.md/main.o sun4.md/obstack.o sun4.md/pinsn.o sun4.md/printcmd.o sun4.md/ptrace.o sun4.md/readline.o sun4.md/remote.o sun4.md/source.o sun4.md/stack.o sun4.md/symmisc.o sun4.md/symtab.o sun4.md/utils.o sun4.md/valarith.o sun4.md/valops.o sun4.md/valprint.o sun4.md/values.o sun4.md/version.o
CLEANOBJS	= sun4.md/blockframe.o sun4.md/breakpoint.o sun4.md/command.o sun4.md/dbxread.o sun4.md/environ.o sun4.md/eval.o sun4.md/expprint.o sun4.md/findvar.o sun4.md/infcmd.o sun4.md/inflow.o sun4.md/infrun.o sun4.md/main.o sun4.md/obstack.o sun4.md/printcmd.o sun4.md/remote.o sun4.md/source.o sun4.md/stack.o sun4.md/symmisc.o sun4.md/symtab.o sun4.md/utils.o sun4.md/valarith.o sun4.md/valops.o sun4.md/valprint.o sun4.md/values.o sun4.md/version.o sun4.md/kgdbcmd.o sun4.md/core.o sun4.md/readline.o sun4.md/dep.o sun4.md/pinsn.o sun4.md/expread.o sun4.md/initialized_all_files.o sun4.md/history.o sun4.md/copying.o sun4.md/ptrace.o
INSTFILES	= sun4.md/md.mk sun4.md/dependencies.mk Makefile local.mk
SACREDOBJS	= 
