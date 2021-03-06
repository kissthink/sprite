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
# Thu Jul 26 18:51:39 PDT 1990
#
# For more information, refer to the mkmf manual page.
#
# $Header: /sprite/lib/mkmf/RCS/Makefile.md,v 1.6 90/03/12 23:28:42 jhh Exp $
#
# Allow mkmf

SRCS		= _exit.c access.c brk.c chdir.c chmod.c chown.c close.c compatMapCode.c compatSig.c creat.c dup.c errno.c execve.c fcntl.c flock.c fork.c fsync.c ftruncate.c getdirentries.c getdtablesize.c geteuid.c getgid.c getgroups.c gethostname.c getitimer.c getpagesize.c getpgrp.c getpid.c getppid.c getpriority.c getrlimit.c getrusage.c gettimeofday.c getuid.c ioctl.c kill.c killpg.c link.c lseek.c mkdir.c mknod.c mmap.c open.c pipe.c profil.c read.c readlink.c readv.c rename.c rmdir.c select.c semop.c setfiletype.c setgroups.c setpgrp.c setpriority.c setregid.c setreuid.c setrlimit.c settimeofday.c sigblock.c sigpause.c sigsetmask.c sigvec.c socket.c stat.c symlink.c tell.c truncate.c umask.c unlink.c utimes.c vfork.c wait.c write.c writev.c
HDRS		= compatInt.h compatSig.h
MDPUBHDRS	= 
OBJS		= ds3100.md/_exit.o ds3100.md/access.o ds3100.md/brk.o ds3100.md/chdir.o ds3100.md/chmod.o ds3100.md/chown.o ds3100.md/close.o ds3100.md/compatMapCode.o ds3100.md/compatSig.o ds3100.md/creat.o ds3100.md/dup.o ds3100.md/errno.o ds3100.md/execve.o ds3100.md/fcntl.o ds3100.md/flock.o ds3100.md/fork.o ds3100.md/fsync.o ds3100.md/ftruncate.o ds3100.md/getdirentries.o ds3100.md/getdtablesize.o ds3100.md/geteuid.o ds3100.md/getgid.o ds3100.md/getgroups.o ds3100.md/gethostname.o ds3100.md/getitimer.o ds3100.md/getpagesize.o ds3100.md/getpgrp.o ds3100.md/getpid.o ds3100.md/getppid.o ds3100.md/getpriority.o ds3100.md/getrlimit.o ds3100.md/getrusage.o ds3100.md/gettimeofday.o ds3100.md/getuid.o ds3100.md/ioctl.o ds3100.md/kill.o ds3100.md/killpg.o ds3100.md/link.o ds3100.md/lseek.o ds3100.md/mkdir.o ds3100.md/mknod.o ds3100.md/mmap.o ds3100.md/open.o ds3100.md/pipe.o ds3100.md/profil.o ds3100.md/read.o ds3100.md/readlink.o ds3100.md/readv.o ds3100.md/rename.o ds3100.md/rmdir.o ds3100.md/select.o ds3100.md/semop.o ds3100.md/setfiletype.o ds3100.md/setgroups.o ds3100.md/setpgrp.o ds3100.md/setpriority.o ds3100.md/setregid.o ds3100.md/setreuid.o ds3100.md/setrlimit.o ds3100.md/settimeofday.o ds3100.md/sigblock.o ds3100.md/sigpause.o ds3100.md/sigsetmask.o ds3100.md/sigvec.o ds3100.md/socket.o ds3100.md/stat.o ds3100.md/symlink.o ds3100.md/tell.o ds3100.md/truncate.o ds3100.md/umask.o ds3100.md/unlink.o ds3100.md/utimes.o ds3100.md/vfork.o ds3100.md/wait.o ds3100.md/write.o ds3100.md/writev.o
CLEANOBJS	= ds3100.md/_exit.o ds3100.md/access.o ds3100.md/brk.o ds3100.md/chdir.o ds3100.md/chmod.o ds3100.md/chown.o ds3100.md/close.o ds3100.md/compatMapCode.o ds3100.md/compatSig.o ds3100.md/creat.o ds3100.md/dup.o ds3100.md/errno.o ds3100.md/execve.o ds3100.md/fcntl.o ds3100.md/flock.o ds3100.md/fork.o ds3100.md/fsync.o ds3100.md/ftruncate.o ds3100.md/getdirentries.o ds3100.md/getdtablesize.o ds3100.md/geteuid.o ds3100.md/getgid.o ds3100.md/getgroups.o ds3100.md/gethostname.o ds3100.md/getitimer.o ds3100.md/getpagesize.o ds3100.md/getpgrp.o ds3100.md/getpid.o ds3100.md/getppid.o ds3100.md/getpriority.o ds3100.md/getrlimit.o ds3100.md/getrusage.o ds3100.md/gettimeofday.o ds3100.md/getuid.o ds3100.md/ioctl.o ds3100.md/kill.o ds3100.md/killpg.o ds3100.md/link.o ds3100.md/lseek.o ds3100.md/mkdir.o ds3100.md/mknod.o ds3100.md/mmap.o ds3100.md/open.o ds3100.md/pipe.o ds3100.md/profil.o ds3100.md/read.o ds3100.md/readlink.o ds3100.md/readv.o ds3100.md/rename.o ds3100.md/rmdir.o ds3100.md/select.o ds3100.md/semop.o ds3100.md/setfiletype.o ds3100.md/setgroups.o ds3100.md/setpgrp.o ds3100.md/setpriority.o ds3100.md/setregid.o ds3100.md/setreuid.o ds3100.md/setrlimit.o ds3100.md/settimeofday.o ds3100.md/sigblock.o ds3100.md/sigpause.o ds3100.md/sigsetmask.o ds3100.md/sigvec.o ds3100.md/socket.o ds3100.md/stat.o ds3100.md/symlink.o ds3100.md/tell.o ds3100.md/truncate.o ds3100.md/umask.o ds3100.md/unlink.o ds3100.md/utimes.o ds3100.md/vfork.o ds3100.md/wait.o ds3100.md/write.o ds3100.md/writev.o
INSTFILES	= ds3100.md/md.mk ds3100.md/dependencies.mk Makefile
SACREDOBJS	= 
