# Generated automatically from Makefile.in by configure.
# Makefile for GNU xargs.
# Do not use this makefile directly, but only from `../Makefile'.
# Copyright (C) 1990 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

SHELL = /bin/sh

srcdir = .
VPATH = .

SOURCES = xargs.c
OBJECTS = xargs.o
DISTFILES = Makefile.in $(SOURCES)

all: xargs
.PHONY: all

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFS) -I$(srcdir)/../lib $<

install: all
	$(INSTALL_PROGRAM) xargs $(bindir)/$(binprefix)xargs
.PHONY: install

TAGS: $(SOURCES)
	etags $(SOURCES)

clean:
	rm -f xargs *.o core
.PHONY: clean

mostlyclean: clean
.PHONY: mostlyclean

distclean: clean
	rm -f Makefile
.PHONY: distclean

realclean: distclean
	rm -f TAGS
.PHONY: realclean

dist:
	ln $(DISTFILES) ../`cat ../.fname`/xargs
.PHONY: dist

xargs: $(OBJECTS) ../lib/libfind.a
	$(CC) -o $@ $(LDFLAGS) $(OBJECTS) ../lib/libfind.a $(LIBS)

xargs.o: ../lib/wait.h

# Prevent GNU make v3 from overflowing arg limit on SysV.
.NOEXPORT:
