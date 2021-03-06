# @(#)@ Makefile 4.2.6 - Makefile for Perl Reference Guide

# This Makefile requires perl to be in your PATH.

SHELL	= /bin/sh

# Delete the next line if you use US Letter paper format
PAPER	= -a4

all:	refguide.ps

2pass:	guide-odd.ps guide-even1.ps guide-even2.ps

# 2 pages per page, suitable for centrefold printing.
refguide.ps:	refbase.ps parr
	perl ./parr $(PAPER) \
		-o 21,2,3,20,19,4,5,18,17,6,7,16,15,8,9,14,13,10,11,12 \
		refbase.ps > refguide.ps

# Odd and even passes for centerfold printing. 
# First print guide-odd.ps, then find out which of the others to use.
# guide-even1.ps is for printers with correct output stacking like
# Apple LaserWriter II. 
# guide-even2.ps for printers with reverse output stacking, like old
# Apple LaserWriters. 

guide-odd.ps:	refbase.ps parr
	perl ./parr $(PAPER) \
		-o 21,2,19,4,17,6,15,8,13,10 refbase.ps > guide-odd.ps

guide-even1.ps:	refbase.ps parr
	perl ./parr $(PAPER) \
		-o 11,12,9,14,7,16,5,18,3,20 refbase.ps > guide-even1.ps

guide-even2.ps:	refbase.ps parr
	perl ./parr $(PAPER) \
		-o 3,20,5,18,7,16,9,14,11,12 refbase.ps > guide-even2.ps

clean:
	rm -f refguide.ps guide-odd.ps guide-even1.ps guide-even2.ps \
		refbase.dvi core *~

# For maintenance use only ...

VER	= 4.019

CH	= ch00.tex ch01.tex ch02.tex ch03.tex ch04.tex ch05.tex \
	  ch06.tex ch07.tex ch08.tex ch09.tex ch10.tex ch11.tex \
	  ch12.tex ch13.tex ch14.tex ch15.tex ch16.tex ch17.tex \
	  ch18.tex ch19.tex ch20.tex ch21.tex ch22.tex ch23.tex \
	  ch24.tex ch25.tex ch26.tex \
	  ch99.tex
SRC	= refbase.tex refbase.sty
AUX	= README ChangeLog Makefile parr Layout

# Our TeX places its output here, never mind if yours doesn't.
TEXTMP = /usr/tmp/$(USER)TeX.

# NOTE: DO NOT REMOVE OR CHANGE '-ta4' EVEN IF USING NON-A4 PAPER
refbase-ps:	refbase.dvi
	dvips -r0 -ta4 refbase.dvi 
	-if [ -f psneat.pl ]; then \
	    mv refbase.ps refbase.ps~; \
	    perl ./psneat.pl refbase.ps~ > refbase.ps; \
	fi

refbase.dvi:	$(SRC)
	latex refbase.tex < /dev/null
	-if [ -f $(TEXTMP)dvi ]; then \
	    mv $(TEXTMP)dvi refbase.dvi; \
	fi

MASTER  = ref
refbase-tex:  $(MASTER)master.tex $(CH) makebase.pl
	perl ./makebase.pl $(REV) $(MASTER)master.tex > refbase.tex

kit:	
	REV=`cat Revision.SEQ`; \
	expr $$REV + 1 >Revision.SEQ; \
	(echo ""; echo Revision: $(VER).$$REV; what $(SRC) $(CH)) >> Revision; \
	$(MAKE) -$(MAKEFLAGS) REV=$$REV refbase-tex refbase-ps kitinternal

kitinternal:
	rm -f perlref_shr.* perlref_ltx.* perlref-$(VER).$(REV).tar.Z
	shar -c -n perlref-$(VER).$(REV) -a -s 'jv@mh.nl' \
		-o perlref-$(VER).$(REV)_shr -L30 -f $(AUX) refbase.ps
	shar -c -n perlltx-$(VER).$(REV) -a -s 'jv@mh.nl' \
		-o perlref-$(VER).$(REV)_ltx -L30 -f $(AUX) $(SRC)
	pdtar -zcvf perlref-$(VER).$(REV).tar.Z $(AUX) $(SRC) refbase.ps
	ls -l perlref-*_shr.* perlref-*_ltx.* perlref-*.tar.Z

ident:
	what $(AUX) $(SRC)
