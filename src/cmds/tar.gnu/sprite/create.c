/* Create a tar archive.
   Copyright (C) 1988 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Create a tar archive.
 *
 * Written 25 Aug 1985 by John Gilmore, ihnp4!hoptoad!gnu.
 *
 * @(#)create.c 1.36 11/6/87 - gnu
 *
 * $Header: /sprite/src/cmds/tar.gnu/sprite/RCS/create.c,v 1.5 92/03/28 17:31:24 kupfer Exp $ SPRITE (Berkeley)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <assert.h>

/* JF: this one is my fault */
/* #include "utils.h" */

#ifndef V7
#include <fcntl.h>
#endif

#ifndef	MSDOS
#include <pwd.h>
#include <grp.h>
#endif

#ifdef BSD42
#include <sys/dir.h>
#else
#ifdef MSDOS
#include <sys/dir.h>
#else
#ifdef USG
#include "dirent.h"
#define direct dirent
#define DP_NAMELEN(x) strlen((x)->d_name)
#else
/*
 * FIXME: On other systems there is no standard place for the header file
 * for the portable directory access routines.  Change the #include line
 * below to bring it in from wherever it is.
 */
#include "ndir.h"
#endif
#endif
#endif

#ifndef DP_NAMELEN
#define DP_NAMELEN(x)	(x)->d_namlen
#endif

#ifdef USG
#include <sys/sysmacros.h>	/* major() and minor() defined here */
#endif

/*
 * V7 doesn't have a #define for this.
 */
#ifndef O_RDONLY
#define	O_RDONLY	0
#endif

/*
 * Most people don't have a #define for this.
 */
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

#include "tar.h"
#include "port.h"

extern union record *head;		/* Points to current tape header */
extern struct stat hstat;		/* Stat struct corresponding */
extern int head_standard;		/* Tape header is in ANSI format */

/* JF */
extern struct name *gnu_list_name;

/*
 * If there are no symbolic links, there is no lstat().  Use stat().
 */
#ifndef S_IFLNK
#define lstat stat
#endif

extern char	*malloc();
extern char	*strcpy();
extern char	*strncpy();
extern void	bzero();
extern void	bcopy();
extern int	errno;

extern void print_header();

union record *start_header();
void finish_header();
void finduname();
void findgname();
char *name_next();
void to_oct();
void dump_file();

static nolinks;			/* Gets set if we run out of RAM */

#ifdef __STDC__
static int copy_long_name(char *name, int type);
#else
static int copy_long_name();
#endif

static char longname[MAXPATHLEN+1];

/*
 * "Scratch" space to store the information about a sparse file before
 * writing the info into the header or extended header
 */
/* struct sp_array	 *sparsearray;*/

/* number of elts storable in the sparsearray */
/*int 	sparse_array_size = 10;*/

void
create_archive()
{
	register char	*p;
	char *name_from_list();

	open_archive(0);		/* Open for writing */

	if(f_gnudump) {
		char buf[MAXNAMLEN],*q,*bufp;

		collect_and_sort_names();

		while(p=name_from_list())
			dump_file(p,-1);
		/* if(!f_dironly) { */
			blank_name_list();
			while(p=name_from_list()) {
				strcpy(buf,p);
				if(p[strlen(p)-1]!='/')
					strcat(buf,"/");
				bufp=buf+strlen(buf);
				for(q=gnu_list_name->dir_contents;*q;q+=strlen(q)+1) {
					if(*q=='Y') {
						strcpy(bufp,q+1);
						dump_file(buf,-1);
					}
				}
			}
		/* } */

	} else {
		while (p = name_next(1)) {
			dump_file(p, -1);
		}
	}

	write_eot();
	close_archive();
	name_close();
}

/*
 * Dump a single file.  If it's a directory, recurse.
 * Result is 1 for success, 0 for failure.
 * Sets global "hstat" to stat() output for this file.
 */
void
dump_file (p, curdev)
	char	*p;			/* File name to dump */
	int	curdev;			/* Device our parent dir was on */
{
	union record	*header;
	char type;
	extern char *save_name;		/* JF for multi-volume support */
	extern long save_totsize;
	extern long save_sizeleft;
	union record	*exhdr;
	char save_linkflag;
	extern time_t new_time;


	if(f_confirm && !confirm("add",p))
		return;

	/*
	 * Use stat if following (rather than dumping) 4.2BSD's
	 * symbolic links.  Otherwise, use lstat (which, on non-4.2
	 * systems, is #define'd to stat anyway.
	 */
	if (0 != f_follow_links? stat(p, &hstat): lstat(p, &hstat))
	{
badperror:
		msg_perror("can't add file %s",p);
badfile:
		errors++;
		return;
	}

	/* See if we only want new files, and check if this one is too old to
	   put in the archive. */
	if(   f_new_files
	   && new_time>hstat.st_mtime
	   && (hstat.st_mode&S_IFMT)!=S_IFDIR
	   && (f_new_files>1 || new_time>hstat.st_ctime)) {
		if(curdev<0) {
			msg("%s: is unchanged; not dumped",p);
		}
		return;
	}

	/*
	 * See if we are crossing from one file system to another,
	 * and avoid doing so if the user only wants to dump one file system.
	 */
	if (f_local_filesys && curdev >= 0 && curdev != hstat.st_dev) {
		msg("%s: is on a different filesystem; not dumped",p);
		return;
	}

	/*
	 * Check for multiple links.
	 *
	 * We maintain a list of all such files that we've written so
	 * far.  Any time we see another, we check the list and
	 * avoid dumping the data again if we've done it once already.
	 */
	if (hstat.st_nlink > 1) switch (hstat.st_mode & S_IFMT) {
		register struct link	*lp;

	case S_IFREG:			/* Regular file */
#ifdef S_IFCTG
	case S_IFCTG:			/* Contigous file */
#endif
#ifdef S_IFCHR
	case S_IFCHR:			/* Character special file */
#endif

#ifdef S_IFBLK
	case S_IFBLK:			/* Block     special file */
#endif

#ifdef S_IFIFO
	case S_IFIFO:			/* Fifo      special file */
#endif

		/* First quick and dirty.  Hashing, etc later FIXME */
		for (lp = linklist; lp; lp = lp->next) {
			if (lp->ino == hstat.st_ino &&
			    lp->dev == hstat.st_dev) {
				char *link_name = lp->name;

				/* We found a link. */
				hstat.st_size = 0;
				header = start_header(p, &hstat);
				if (header == NULL) goto badfile;
				while(!f_absolute_paths && *link_name == '/') {
					static int link_warn = 0;

					if (!link_warn) {
						msg("Removing leading / from absolute links");
						link_warn++;
					}
					link_name++;
				}
				header->header.linkflag = LF_LINK;
				/* 
				 * BSD tar requires that the link name have 
				 * a trailing null.  See the comments in 
				 * start_header.
				 */
				if (strlen(link_name) < NAMSIZ) {
					strcpy(header->header.linkname,
					       link_name);
					finish_header(header);
				} else if (!f_long_names) {
					msg("link name ``%s'' too long.",
					    link_name);
					return;
				} else {
					header->header.isextended |= XH_LINKNAME;
					strcpy(header->header.linkname,
					       "LINK_TOO_LONG--CHECK_EXTENDED_HEADER");
					finish_header(header);
					copy_long_name(link_name, XH_LINKNAME);
				}
		/* FIXME: Maybe remove from list after all links found? */
				return;		/* We dumped it */
			}
		}

		/* Not found.  Add it to the list of possible links. */
		lp = (struct link *) malloc( (unsigned)
			(strlen(p) + sizeof(struct link) - NAMSIZ));
		if (!lp) {
			if (!nolinks) {
				fprintf(stderr,
	"tar: no memory for links, they will be dumped as separate files\n");
				nolinks++;
			}
		}
		lp->ino = hstat.st_ino;
		lp->dev = hstat.st_dev;
		strcpy(lp->name, p);
		lp->next = linklist;
		linklist = lp;
	}

	/*
	 * This is not a link to a previously dumped file, so dump it.
	 */
	switch (hstat.st_mode & S_IFMT) {

	case S_IFREG:			/* Regular file */
#ifdef S_IFCTG
	case S_IFCTG:			/* Contiguous file */
#endif
	{
		int	f;		/* File descriptor */
		long	bufsize, count;
		long	sizeleft;
		register union record 	*start;
		int 	header_moved;
		char	isextended = 0;
		int 	upperbound;
		int	end_nulls = 0;

		header_moved = 0;

#ifdef BSD42
		if (f_sparse_files) {
		/*
		 * JK - This is the test for sparseness: whether the
		 * "size" of the file matches the number of blocks
		 * allocated for it.  If there is a smaller number
		 * of blocks that would be necessary to accommodate
		 * a file of this size, we have a sparse file, i.e.,
		 * at least one of those records in the file is just
		 * a useless hole.
		 */
			if (hstat.st_size - (hstat.st_blocks * RECORDSIZE) > RECORDSIZE) {
				int	filesize = hstat.st_size;
				register int i;
				
				printf("File is sparse: %s\n", p);
				header = start_header(p, &hstat);
				if (header == NULL)
					goto badfile;
				header->header.linkflag = LF_SPARSE;
				header_moved++;
				
			/*
			 * Call the routine that figures out the
			 * layout of the sparse file in question.
			 * UPPERBOUND is the index of the last
			 * element of the "sparsearray," i.e.,
			 * the number of elements it needed to
			 * describe the file.
			 */

				upperbound = deal_with_sparse(p, header);

			/* 
			 * See if we'll need an extended header
			 * later
			 */
			        if (upperbound > SPARSE_IN_HDR-1)
				    header->header.isextended |= XH_SPARSE_FILE;

			/*
			 * We store the "real" file size so
			 * we can show that in case someone wants
			 * to list the archive, i.e., tar tvf <file>.
			 * It might be kind of disconcerting if the
			 * shrunken file size was the one that showed
			 * up.
			 */
				 to_oct((long) hstat.st_size, 1+12, 
						header->header.realsize);
					
			/*
			 * This will be the new "size" of the
			 * file, i.e., the size of the file
			 * minus the records of holes that we're
			 * skipping over. 
			 */
				 
				find_new_file_size(&filesize, upperbound);
				printf("File %s is now size %d\n", 
							p, filesize);
				hstat.st_size = filesize;
				to_oct((long) filesize, 1+12,
 						header->header.size);
/*				to_oct((long) end_nulls, 1+12, 
						header->header.ending_blanks);*/
						
				for (i = 0; i < SPARSE_IN_HDR; i++) {
					if (!sparsearray[i].numbytes)
						break;
					to_oct(sparsearray[i].offset, 1+12,
						header->header.sp[i].offset);
					to_oct(sparsearray[i].numbytes, 1+12,
						header->header.sp[i].numbytes);
				}
					
			}
		}
#endif
		
		sizeleft = hstat.st_size;
		/* Don't bother opening empty, world readable files. */
		if (sizeleft > 0 || 0444 != (0444 & hstat.st_mode)) {
			f = open(p, O_RDONLY|O_BINARY);
			if (f < 0) goto badperror;
		} else {
			f = -1;
		}

		/* If the file is sparse, we've already taken care of this */
		if (!header_moved) {
			header = start_header(p, &hstat);
			if (header == NULL) {
				if(f>=0)
					(void)close(f);
				goto badfile;
			}
		}
#ifdef S_IFCTG
		/* Mark contiguous files, if we support them */
		if (f_standard && (hstat.st_mode & S_IFMT) == S_IFCTG) {
			header->header.linkflag = LF_CONTIG;
		}
#endif
		save_linkflag = header->header.linkflag;
		finish_header(header);
		if (isextended) {
			int	 sum = 0;
			register int i;
/*			register union record *exhdr;*/
			static int index_offset = SPARSE_IN_HDR;

	extend:		exhdr = findrec();

			if (exhdr == NULL) goto badfile;
			bzero(exhdr->charptr, RECORDSIZE);
			exhdr->ext_hdr.xh_type = XH_SPARSE_FILE;
			for (i = 0; i < SPARSE_EXT_HDR; i++) {
				if (i+index_offset > upperbound)
					break;
				to_oct((long) sparsearray[i+index_offset].numbytes,
					1+12,
					exhdr->ext_hdr.xh_sp[i].numbytes);
				to_oct((long) sparsearray[i+index_offset].offset,
					1+12,
					exhdr->ext_hdr.xh_sp[i].offset);
			}
			userec(exhdr);
/*			sum += i;
			if (sum < upperbound)
				goto extend;*/
			if (index_offset+i < upperbound) {
				index_offset += i;
				exhdr->ext_hdr.xh_isextended |= XH_SPARSE_FILE;
				goto extend;
			}

		}
		if (save_linkflag == LF_SPARSE) {
			if (finish_sparse_file(f, &sizeleft, hstat.st_size, p))
				goto padit;
		}
		else
		  while (sizeleft > 0) {

			if(f_multivol) {   
				save_name = p;
				save_sizeleft = sizeleft;
				save_totsize = hstat.st_size;
			}
			start = findrec();

			bufsize = endofrecs()->charptr - start->charptr;

			if (sizeleft < bufsize) {
				/* Last read -- zero out area beyond */
				bufsize = (int)sizeleft;
				count = bufsize % RECORDSIZE;
				if (count) 
					bzero(start->charptr + sizeleft,
						(int)(RECORDSIZE - count));
			}
			count = read(f, start->charptr, bufsize);
			if (count < 0) {
				msg_perror("read error at byte %ld, reading\
 %d bytes, in file %s",  hstat.st_size - sizeleft, bufsize,p);
				goto padit;
			}
			sizeleft -= count;

			/* This is nonportable (the type of userec's arg). */
			userec(start+(count-1)/RECORDSIZE);

			if (count == bufsize) continue;
			msg( "file %s shrunk by %d bytes, padding with zeros.\n", p, sizeleft);
			goto padit;		/* Short read */
		}

		if(f_multivol)
			save_name = 0;

		if (f >= 0)
			(void)close(f);

		break;

		/*
		 * File shrunk or gave error, pad out tape to match
		 * the size we specified in the header.
		 */
	padit:
		while(sizeleft>0) {
			save_sizeleft=sizeleft;
			start=findrec();
			bzero(start->charptr,RECORDSIZE);
			userec(start);
			sizeleft-=RECORDSIZE;
		}
		if(f_multivol)
			save_name=0;
		if(f>=0)
			(void)close(f);
		break;
/*		abort(); */
	}

#ifdef S_IFLNK
	case S_IFLNK:			/* Symbolic link */
	{
		int size;
		char linknamebuf[MAXPATHLEN+1]; /* +1 for null */

		hstat.st_size = 0;		/* Force 0 size on symlink */
		header = start_header(p, &hstat);
		if (header == NULL)
			goto badfile;
		size = readlink(p, linknamebuf, NAMSIZ);
		if (size < 0)
			goto badperror;
		linknamebuf[size] = '\0';
		header->header.linkflag = LF_SYMLINK;
		/* 
		 * BSD tar requires that the link name have a trailing 
		 * null.  See the comments in start_header.
		 */
		if (size < NAMSIZ) {
			strcpy(header->header.linkname, linknamebuf);
			finish_header(header);	/* Nothing more to do to it */
		} else if (!f_long_names) {
			msg("symbolic link %s too long\n",p);
			break;
		} else {
			size = readlink(p, linknamebuf, MAXPATHLEN);
			linknamebuf[size] = '\0';
			header->header.isextended |= XH_LINKNAME;
			strcpy(header->header.linkname,
			       "LINK_TOO_LONG--CHECK_EXTENDED_HEADER");
			finish_header(header);
			copy_long_name(linknamebuf, XH_LINKNAME);
		}
	}
		break;
#endif
#ifdef S_IFRLNK
	case S_IFRLNK:			/* Remote link */
	{
		int size;
		char linknamebuf[MAXPATHLEN+1]; /* +1 for null */

		hstat.st_size = 0;		/* Force 0 size on symlink */
		header = start_header(p, &hstat);
		if (header == NULL)
			goto badfile;
		size = readlink(p,linknamebuf, NAMSIZ+1);
		if (size < 0)
			goto badperror;
		linknamebuf[size] = '\0';
		header->header.linkflag = LF_RMTLINK;
		/* 
		 * BSD tar requires that the link name have a trailing 
		 * null.  See the comments in start_header.
		 */
		if (size < NAMSIZ) {
			strcpy(header->header.linkname, linknamebuf);
			finish_header(header);	/* Nothing more to do to it */
		} else if (!f_long_names) {
			msg("%s: remote link too long\n", p);
			break;
		} else {
			size = readlink(p, linknamebuf, MAXPATHLEN);
			linknamebuf[size] = '\0';
			header->header.isextended |= XH_LINKNAME;
			strcpy(header->header.linkname,
			       "LINK_TOO_LONG--CHECK_EXTENDED_HEADER");
			finish_header(header);
			copy_long_name(linknamebuf, XH_LINKNAME);
		}
	}
		break;
#endif

#ifdef S_IFPDEV                         /* Psuedo device */
	case S_IFPDEV:
		hstat.st_size = 0;
		header = start_header(p, &hstat);
		if (header == NULL) goto badfile;
		header->header.linkflag = LF_PSEUDODEV;
		finish_header(header);
		break;
#endif

	case S_IFDIR:			/* Directory */
	    {
		register DIR *dirp;
		register struct direct *d;
#ifdef ALLOW_LONG_NAMES		
		char namebuf[MAXPATHLEN+2];
#else		
		char namebuf[NAMSIZ+2];
#endif		
		register int len;
		int our_device = hstat.st_dev;

		/* Build new prototype name */
		strncpy(namebuf, p, sizeof (namebuf));
		len = strlen(namebuf);
		while (len >= 1 && '/' == namebuf[len-1]) 
			len--;			/* Delete trailing slashes */
		namebuf[len++] = '/';		/* Now add exactly one back */
		namebuf[len] = '\0';		/* Make sure null-terminated */

		/*
		 * Output directory header record with permissions
		 * FIXME, do this AFTER files, to avoid R/O dir problems?
		 * If old archive format, don't write record at all.
		 */
		if (!f_oldarch) {
			hstat.st_size = 0;	/* Force 0 size on dir */
			/*
			 * If people could really read standard archives,
			 * this should be:		(FIXME)
			header = start_header(f_standard? p: namebuf, &hstat);
			 * but since they'd interpret LF_DIR records as
			 * regular files, we'd better put the / on the name.
			 */
			header = start_header(namebuf, &hstat);
			if (header == NULL)
				goto badfile;	/* eg name too long */

			if (f_gnudump)
				header->header.linkflag = LF_DUMPDIR;
			else if (f_standard)
				header->header.linkflag = LF_DIR;

			/* If we're gnudumping, we aren't done yet so don't close it. */
			if(!f_gnudump)
				finish_header(header);	/* Done with directory header */
		}

		/* Hack to remove "./" from the front of all the file names */
		if (len == 2 && namebuf[0] == '.' && namebuf[1]=='/') {
			len = 0;
		}

		if(f_gnudump) {
			int sizeleft;
			int totsize;
			int bufsize;
			union record *start;
			int count;
			char *buf,*p_buf;

			buf=gnu_list_name->dir_contents; /* FOO */
			totsize=0;
			for(p_buf=buf;*p_buf;) {
				int tmp;

				tmp=strlen(p_buf)+1;
				totsize+=tmp;
				p_buf+=tmp;
			}
			totsize++;
			to_oct((long)totsize,1+12,header->header.size);
			finish_header(header);
			p_buf=buf;
			sizeleft=totsize;
			while(sizeleft>0) {
				if(f_multivol) {
					save_name=p;
					save_sizeleft=sizeleft;
					save_totsize=totsize;
				}
				start=findrec();
				bufsize=endofrecs()->charptr - start->charptr;
				if(sizeleft<bufsize) {
					bufsize=sizeleft;
					count=bufsize%RECORDSIZE;
					if(count)
						bzero(start->charptr+sizeleft,RECORDSIZE-count);
				}
				bcopy(p_buf,start->charptr,bufsize);
				sizeleft-=bufsize;
				p_buf+=bufsize;
				userec(start+(bufsize-1)/RECORDSIZE);
			}
			if(f_multivol)
				save_name = 0;
			break;
		}

		/* Now output all the files in the directory */
#ifdef ALLOW_NO_RECURSE
		if (f_no_recurse) {
		    break;		/* Unless the cmdline said not to */
		}
#endif	    
		errno = 0;
		dirp = opendir(p);
		if (!dirp) {
			if (errno) {
				msg_perror ("can't open directory %s",p);
			} else {
				msg("error opening directory %s",
					p);
			}
			break;
		}

		/* Should speed this up by cd-ing into the dir, FIXME */
		while (NULL != (d=readdir(dirp))) {
			/* Skip . and .. */
			if(is_dot_or_dotdot(d->d_name))
				continue;
#ifdef ALLOW_LONG_NAMES
			if (DP_NAMELEN(d) + len >= MAXPATHLEN) {
#else			    
			if (DP_NAMELEN(d) + len >= NAMSIZ) {
#endif			    
				msg("file name %s%s too long\n", 
					namebuf, d->d_name);
				continue;
			}
			strcpy(namebuf+len, d->d_name);
			if(f_exclude && check_exclude(namebuf))
				continue;
			dump_file(namebuf, our_device);
		}

		closedir(dirp);
	}
		break;

#ifdef S_IFCHR
	case S_IFCHR:			/* Character special file */
		type = LF_CHR;
		goto easy;
#endif

#ifdef S_IFBLK
	case S_IFBLK:			/* Block     special file */
		type = LF_BLK;
		goto easy;
#endif

#if defined (sprite) && defined (S_IFSOCK)
	case S_IFSOCK:
	    /*
	     * Fall through and do a named pipe.  The Sprite compatible
	     * stat routine maps Sprite named pipes to S_IFSOCK.
	     */
#endif
#ifdef S_IFIFO
	case S_IFIFO:			/* Fifo      special file */

		type = LF_FIFO;
#endif

	easy:
		if (!f_standard) goto unknown;

		hstat.st_size = 0;		/* Force 0 size */
		header = start_header(p, &hstat);
		if (header == NULL) goto badfile;	/* eg name too long */

		header->header.linkflag = type;
		if (type != LF_FIFO) {
			to_oct((long) major(hstat.st_rdev), 8,
				header->header.devmajor);
			to_oct((long) minor(hstat.st_rdev), 8,
				header->header.devminor);
		}

		finish_header(header);
		break;

	default:
	unknown:
		msg("%s: Unknown file type; file ignored.\n", p);
		break;
	}
}

int
finish_sparse_file(fd, sizeleft, fullsize, name)
	int	fd;
	long 	*sizeleft,
		fullsize;
	char	*name;
{
	union record	*start;
	char		tempbuf[RECORDSIZE];
	int		bufsize,
			sparse_ind = 0,
			count;
	long		pos;



	while (*sizeleft > 0) {
		start = findrec();
		bzero(start->charptr, RECORDSIZE);
		bufsize = sparsearray[sparse_ind].numbytes;
		if (!bufsize) {  /* we blew it, maybe */
		        fprintf(stderr, "Wrote %ld of %ld bytes to file %s\n",
			           fullsize - *sizeleft, fullsize, name);
			break;
		}
		pos = lseek(fd, sparsearray[sparse_ind++].offset, 0);
		/* 
		 * If the number of bytes to be written here exceeds
		 * the size of the temporary buffer, do it in steps.
		 */
		while (bufsize > RECORDSIZE) {
/*			if (amt_read) {
				count = read(fd, start->charptr+amt_read, RECORDSIZE-amt_read);
				bufsize -= RECORDSIZE - amt_read;
				amt_read = 0;
				userec(start);
				start = findrec();
				bzero(start->charptr, RECORDSIZE);
			}*/
			/* store the data */
			count = read(fd, start->charptr, RECORDSIZE);
			if (count < 0) 	{
				msg_perror("read error at byte %ld, reading %d bytes, in file %s", 
						fullsize - *sizeleft, bufsize, name);
				return 1;
			}			
			bufsize -= count;
			*sizeleft -= count;
			userec(start);
			start = findrec();
			bzero(start->charptr, RECORDSIZE);
		}


		clear_buffer(tempbuf);
		count = read(fd, tempbuf, bufsize);
		bcopy(tempbuf, start->charptr, RECORDSIZE);
		if (count < 0) 	{
			msg_perror("read error at byte %ld, reading %d bytes, in file %s", 
					fullsize - *sizeleft, bufsize, name);
			return 1;
		}
/*		if (amt_read >= RECORDSIZE) {
			amt_read = 0;
			userec(start+(count-1)/RECORDSIZE);
			if (count != bufsize) {
				msg("file %s shrunk by %d bytes, padding with zeros.\n", name, sizeleft);
				return 1;
			}
			start = findrec();
		} else 
			amt_read += bufsize;*/
		*sizeleft -= count;
		userec(start);

	}
/*	userec(start+(count-1)/RECORDSIZE);*/
	return 0;

}

init_sparsearray()
{
	register int i;

	for (i = 0; i < sp_array_size; i++) {
		sparsearray[i].offset = 0;
		sparsearray[i].numbytes = 0;
	}
}



/*
 * Okay, we've got a sparse file on our hands -- now, what we need to do is
 * make a pass through the file and carefully note where any data is, i.e.,
 * we want to find how far into the file each instance of data is, and how
 * many bytes are there.  We store this information in the sparsearray,
 * which will later be translated into header information.  For now, we use
 * the sparsearray as convenient storage.
 *
 * As a side note, this routine is a mess.  If I could have found a cleaner
 * way to do it, I would have.  If anyone wants to find a nicer way to do
 * this, feel free.
 */
int
deal_with_sparse(name, header, nulls_at_end)
	char		*name;
	union record 	*header;

{
	long	numbytes = 0;
	long	offset = 0;
	int	fd;
	int	start,
		end;
	int	end_nulls = 0;
	int	sparse_ind = 0,
		cc;
	char	buf[RECORDSIZE];
	int 	amidst_data = 0;

	header->header.isextended = 0;
	/* 
	 * Can't open the file -- this problem will be caught later on,
	 * so just return.
	 */
	if ((fd = open(name, O_RDONLY)) < 0)
		return;

	sp_array_size = 10;
	/* 
	 * Make room for our scratch space -- initially is 10 elts long
	 */
	sparsearray = (struct sp_array *) malloc(sp_array_size * sizeof(struct sp_array));

	init_sparsearray();
	clear_buffer(buf);

	while ((cc = read(fd, buf, sizeof buf)) != 0) {

		if (sparse_ind > sp_array_size-1) {

		/*
		 * realloc the scratch area, since we've run out of room --
		 */
			sparsearray = (struct sp_array *) 
					realloc(sparsearray,
						2 * sp_array_size * (sizeof(struct sp_array)));
			sp_array_size *= 2;
		}
		if (cc == sizeof buf) {
			if (zero_record(buf)) {
				if (amidst_data) {
					sparsearray[sparse_ind++].numbytes
						= numbytes;
					amidst_data = 0;
					numbytes = 0;
				}
				offset += cc;
			} else {  /* !zero_record(buf) */
				if (!amidst_data) {
				        amidst_data = 1;
					where_is_data(&start, &end, buf);
					numbytes += end - start;
					offset += start;
					sparsearray[sparse_ind].offset = offset;
				} else
					numbytes += cc;
				offset += cc;
			}
		} else if (cc < sizeof buf) {
			if (!zero_record(buf)) {
				if (!amidst_data) {
					amidst_data = 1;
					where_is_data(&start, &end, buf);
					/* In case there are nulls at the 
					   end that we need to remember */
					if (end < cc)
						end = cc;
					numbytes += start - end;
					offset += start;
/*					end_nulls = RECORDSIZE - end;*/
				} else {
					numbytes += cc;
/*					end_nulls = RECORDSIZE - end;*/
				}
				sparsearray[sparse_ind].numbytes = numbytes;
			} /* else
				end_nulls = cc;*/
		}
		clear_buffer(buf);
	}
	if (numbytes)
	        sparsearray[sparse_ind].numbytes = numbytes;
	close(fd);
/*	printf("%d\n", end_nulls);
	*nulls_at_end = end_nulls;*/

	return sparse_ind;
}

/* 
 * Just zeroes out the buffer so we don't confuse ourselves with leftover
 * data.
 */
clear_buffer(buf)
	char	*buf;
{
	register int 	i;

	for (i = 0; i < RECORDSIZE; i++)
		buf[i] = '\0';
}

/* 
 * JK - 
 * This routine takes a character array, and tells where within that array
 * the data can be found.  It skips over any zeros, and sets the first
 * non-zero point in the array to be the "start", and continues until it
 * finds non-data again, which is marked as the "end."  This routine is 
 * mainly for 1) seeing how far into a file we must lseek to data, given
 * that we have a sparse file, and 2) determining the "real size" of the
 * file, i.e., the number of bytes in the sparse file that are data, as
 * opposed to the zeros we are trying to skip.
 */
where_is_data(from, to, buffer)
	int	*from,
		*to;
	char	*buffer;
{
	register int	i = 0;
	register int	save_to = *to;
	int	amidst_data = 0;


	while (!buffer[i])
		i++;
	*from = i;

	if (*from < 16)	/* don't bother */
		*from = 0;
	/* keep going to make sure there isn't more real
	   data in this record */
	while (i < RECORDSIZE) {
		if (!buffer[i]) {
			if (amidst_data) {
				save_to = i;
				amidst_data = 0;
			}
			i++;
		}
		else if (buffer[i]) {
			if (!amidst_data)
				amidst_data = 1;
			i++;
		}
	}
	if (i == RECORDSIZE)
		*to = i;
	else
		*to = save_to;

}

/*
 * Takes a recordful of data and basically cruises through it to see if
 * it's made *entirely* of zeros, returning a 0 the instant it finds
 * something that is a non-zero, i.e., useful data.
 */
zero_record(buffer)
	char	*buffer;
{
	register int	i;

	for (i = 0; i < RECORDSIZE; i++)
		if (buffer[i] != '\000')
			return 0;
	return 1;
}

find_new_file_size(filesize, highest_index)
	int	*filesize;
	int	highest_index;
{
	register int 	i;

	*filesize = 0;
	for (i = 0; sparsearray[i].numbytes && i <= highest_index; i++)
		*filesize += sparsearray[i].numbytes;
}

/*
 * Make a header block for the file  name  whose stat info is  st .
 * Return header pointer for success, NULL if the name is too long.
 */
union record *
start_header(name, st)
	char	*name;
	register struct stat *st;
{
	register union record *header;

	header = (union record *) findrec();
	bzero(header->charptr, sizeof(*header)); /* XXX speed up */

	header->header.hdr_magic = HDR_MAGIC;

	/*
	 * Check the file name and put it in the record.
	 */
#ifdef MSDOS
	if(name[1]==':') {
		static int warned_once = 0;
		name+=2;
		if(!warned_once++) {
			msg("Removing drive spec from names in the archive");
		}
	}
#endif
	while (!f_absolute_paths && '/' == *name) {
		static int warned_once = 0;

		name++;				/* Force relative path */
		if (!warned_once++) {
			msg("Removing leading / from absolute path names in the archive.");
		}
	}
	/* 
	 * Posix lets the name take up all NAMSIZ characters, but BSD tar 
	 * requires a trailing null.  We can't pretend to be Posix 
	 * compliant for other reasons, so let's at least try to be
	 * compatible with BSD.
	 */
	if (strlen(name) < NAMSIZ) {
		strcpy(header->header.name, name);
	} else if (!f_long_names) {
		msg("%s: name too long\n", name);
		return NULL;
	} else {
		int size;
		static long unique;
		
		header->header.isextended |= XH_FILENAME;
		size = strlen(name);
		if (size >= MAXPATHLEN) {
			msg("%s: file name exceeds MAXPATHLEN\n", name);
			return NULL;
		}
		sprintf(header->header.name, "/tmp/TAR.NAME_TOO_LONG.%08ld",
			++unique);
		strcpy(longname, name);
	}

	to_oct((long) (st->st_mode & ~S_IFMT),
					8,  header->header.mode);
	to_oct((long) st->st_uid,	8,  header->header.uid);
	to_oct((long) st->st_gid,	8,  header->header.gid);
	to_oct((long) st->st_size,	1+12, header->header.size);
	to_oct((long) st->st_mtime,	1+12, header->header.mtime);
	/* header->header.linkflag is left as null */
	if(f_gnudump) {
		to_oct((long) st->st_atime, 1+12, header->header.atime);
		to_oct((long) st->st_ctime, 1+12, header->header.ctime);
	}

#ifndef NONAMES
	/* Fill in new Unix Standard fields if desired. */
	if (f_standard) {
		header->header.linkflag = LF_NORMAL;	/* New default */
		strcpy(header->header.magic, TMAGIC);	/* Mark as Unix Std */
		finduname(header->header.uname, st->st_uid);
		findgname(header->header.gname, st->st_gid);
	}
#endif
	return header;
}

/* 
 * Finish off a filled-in header block and write it out.
 * We also print the file name and/or full info if verbose is on.
 * 
 * Note: this code should be able to handle Posix names (no terminating 
 * null in the header).
 */
void
finish_header(header)
	register union record *header;
{
	register int	i, sum;
	register char	*p;
	void bcopy();
	char shortname[NAMSIZ+1]; /* copy from header; +1 for null */

	bcopy(CHKBLANKS, header->header.chksum, sizeof(header->header.chksum));

	sum = 0;
	p = header->charptr;
	for (i = sizeof(*header); --i >= 0; ) {
		/*
		 * We can't use unsigned char here because of old compilers,
		 * e.g. V7.
		 */
		sum += 0xFF & *p++;
	}

	/*
	 * Fill in the checksum field.  It's formatted differently
	 * from the other fields:  it has [6] digits, a null, then a
	 * space -- rather than digits, a space, then a null.
	 * We use to_oct then write the null in over to_oct's space.
	 * The final space is already there, from checksumming, and
	 * to_oct doesn't modify it.
	 *
	 * This is a fast way to do:
	 * (void) sprintf(header->header.chksum, "%6o", sum);
	 */
	to_oct((long) sum,	8,  header->header.chksum);
	header->header.chksum[6] = '\0';	/* Zap the space */

	userec(header);

	if (f_verbose) {
		/* These globals are parameters to print_header, sigh */
		head = header;
		/* hstat is already set up */
		head_standard = f_standard;
		if (current_filename != NULL) {
			free(current_filename);
		}
		if (header->header.isextended & XH_FILENAME) {
			current_filename = strdup(longname);
		} else {
			strncpy(shortname, head->header.name, NAMSIZ);
			shortname[NAMSIZ] = '\0';
			current_filename = strdup(shortname);
		}
		/* 
		 * XXX We don't have the link name, but that's okay as long 
		 * as print_header doesn't need it, which is true if 
		 * f_verbose == 1.
		 */
		assert(f_verbose == 1);
		print_header();
	}
	if (!f_long_names) {
		assert((header->header.isextended &
			(XH_FILENAME|XH_LINKNAME)) == 0);
	} else {
		if (header->header.isextended & XH_FILENAME) {
			copy_long_name(longname, XH_FILENAME);
		}
	}
	return;
}

/*
 * Quick and dirty octal conversion.
 * Converts long "value" into a "digs"-digit field at "where",
 * including a trailing space and room for a null.  "digs"==3 means
 * 1 digit, a space, and room for a null.
 *
 * We assume the trailing null is already there and don't fill it in.
 * This fact is used by start_header and finish_header, so don't change it!
 *
 * This should be equivalent to:
 *	(void) sprintf(where, "%*lo ", digs-2, value);
 * except that sprintf fills in the trailing null and we don't.
 */
void
to_oct(value, digs, where)
	register long	value;
	register int	digs;
	register char	*where;
{

	--digs;				/* Trailing null slot is left alone */
	where[--digs] = ' ';		/* Put in the space, though */

	/* Produce the digits -- at least one */
	do {
		where[--digs] = '0' + (char)(value & 7); /* one octal digit */
		value >>= 3;
	} while (digs > 0 && value != 0);

	/* Leading spaces, if necessary */
	while (digs > 0)
		where[--digs] = ' ';

}


/*
 * Write the EOT record(s).
 * We actually zero at least one record, through the end of the block.
 * Old tar writes garbage after two zeroed records -- and PDtar used to.
 */
write_eot()
{
	union record *p;
	int bufsize;
	void bzero();

	p = findrec();
	bufsize = endofrecs()->charptr - p->charptr;
	bzero(p->charptr, bufsize);
	userec(p);
}


static int
copy_long_name(name, type)
    char *name;
    int type;
{
    int size;
    union record *exhdr;

    assert(type == XH_FILENAME || type == XH_LINKNAME);
    size = strlen(name);
    assert(size >= NAMSIZ);
    assert(size <= MAXPATHLEN);
    for (;;) {
	assert(strlen(name) == size);
	exhdr = findrec();
	if (exhdr == NULL) {
	    return 0;		/* XXX nobody actually looks at this number */
	}

	/* 
	 * Make sure that the string will be null-terminated
	 */
	bzero(exhdr, sizeof(exhdr));

	exhdr->ext_hdr.xh_type = type;
	exhdr->ext_hdr.xh_magic = XH_MAGIC;
	strncpy(exhdr->ext_hdr.xh_namebuf, name,
	    sizeof(exhdr->ext_hdr.xh_namebuf));
	if (size >= sizeof(exhdr->ext_hdr.xh_namebuf)) {
	    size -= sizeof(exhdr->ext_hdr.xh_namebuf);
	    name += sizeof(exhdr->ext_hdr.xh_namebuf);
	    exhdr->ext_hdr.xh_isextended |= type;
	    userec(exhdr);
	    continue;
	} else {
	    exhdr->ext_hdr.xh_isextended = 0;
	    userec(exhdr);
	    return 1;
	}
    }
}

