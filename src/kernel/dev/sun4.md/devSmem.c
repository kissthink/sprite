/* 
 * devSmem.c --
 *
 *	Stubs to implement /dev/smem.  Allow reading and writing
 *      to kernel memory.
 *
 *
 * Copyright 1987 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /cdrom/src/kernel/Cvsroot/kernel/dev/sun4.md/devSmem.c,v 9.1 92/08/13 15:53:51 secor Exp $ SPRITE (Berkeley)";
#endif not lint

#include "sprite.h"
#include "fs.h"
#include <vmSunConst.h>
#include <dbg.h>
#define NOGAP

extern int vm_PageSize;
extern int mach_KernStackSize;
extern int vmBlockCacheEndAddr;
extern int vmBlockCacheBaseAddr;
extern int vmStackEndAddr;
extern int vmStackBaseAddr;
extern int vmMemEnd;
extern Address mach_KernStart;
extern Address mach_CodeStart;
extern int end;


/*
 *----------------------------------------------------------------------
 *
 *  Dev_SmemRead --
 *
 *	Return number of bytes read and SUCCESS if nonzero bytes returned.
 *
 * Results:
 *	A standard Sprite return status.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
Dev_SmemRead(devicePtr, readPtr, replyPtr)
    Fs_Device *devicePtr;
    Fs_IOParam	*readPtr;	/* Read parameter block */
    Fs_IOReply	*replyPtr;	/* Return length and signal */ 
{
  int bytesLeft;
  int kernelAddress;
  int numPages, bytesInPage;
  char *bufPtr;
  StopInfo stopInfo;
  Dbg_DumpBounds currentBounds;
  
  stopInfo.codeStart = (int)mach_CodeStart;
  stopInfo.trapType = 0;
/*  stopInfo.regs = ;*/
  currentBounds.pageSize = vm_PageSize;
  currentBounds.stackSize = mach_KernStackSize;
  currentBounds.kernelCodeStart = (unsigned int) mach_KernStart;
  currentBounds.kernelCodeSize  =
    (unsigned int) (((Address)(&end)) - mach_KernStart);
  currentBounds.kernelDataStart  = ((unsigned int)(&end));
  currentBounds.kernelDataSize   = (unsigned int)
    (((Address)(vmMemEnd)) - ((Address)(&end)));
  currentBounds.kernelStacksStart = (unsigned int)vmStackBaseAddr;
  currentBounds.kernelStacksSize = (unsigned int)
    (vmStackEndAddr - vmStackBaseAddr);
  currentBounds.fileCacheStart   = (unsigned int)vmBlockCacheBaseAddr;
  currentBounds.fileCacheSize    = (unsigned int) (vmBlockCacheEndAddr -
					    vmBlockCacheBaseAddr);
  kernelAddress = readPtr->offset;
  bytesLeft = readPtr->length;
  bufPtr = readPtr->buffer;
  if (kernelAddress < sizeof(StopInfo)) {
    if (bytesLeft + kernelAddress <= sizeof(StopInfo)) {
      bcopy(((char *)&stopInfo) + kernelAddress, readPtr->buffer, readPtr->length);
      replyPtr->length = readPtr->length;
      return(SUCCESS);
    }
    bcopy(((char *)&stopInfo) + kernelAddress, readPtr->buffer, sizeof(StopInfo) - kernelAddress);
    bytesLeft -= sizeof(StopInfo) - kernelAddress;
    bufPtr += sizeof(StopInfo) - kernelAddress;
    kernelAddress = sizeof(StopInfo);
  }
  if (kernelAddress < (sizeof(StopInfo) + sizeof(Dbg_DumpBounds))) {
    if (bytesLeft + kernelAddress <= sizeof(StopInfo) + sizeof(Dbg_DumpBounds)) {
      bcopy(((char *)&currentBounds) + sizeof(StopInfo) - kernelAddress, bufPtr, bytesLeft);
      replyPtr->length = readPtr->length;
      return(SUCCESS);
    }
    bcopy(((char *)&currentBounds) + sizeof(StopInfo) - kernelAddress, bufPtr, sizeof(Dbg_DumpBounds));
    bytesLeft -= sizeof(StopInfo) + sizeof(Dbg_DumpBounds) - kernelAddress;
    bufPtr += sizeof(StopInfo) + sizeof(Dbg_DumpBounds) - kernelAddress;
    kernelAddress = mach_KernStart;
  }
  kernelAddress += mach_KernStart - (sizeof(StopInfo) + sizeof(Dbg_DumpBounds));

#ifdef NOGAP
  if (kernelAddress > vmMemEnd) {
    kernelAddress += vmStackBaseAddr - vmMemEnd;
  }
  if (kernelAddress > vmStackEndAddr) {
    kernelAddress += vmBlockCacheBaseAddr - vmStackEndAddr;
  }
  if (kernelAddress > vmBlockCacheEndAddr) {
    return (SYS_INVALID_ARG);
  }
#else
  if (kernelAddress > vmMemEnd &&
      kernelAddress < vmStackBaseAddr) {
    return(SYS_INVALID_ARG);
  }
  if (kernelAddress > vmStackEndAddr &&
      kernelAddress < vmBlockCacheBaseAddr) {
    return(SYS_INVALID_ARG);
  }
  if (kernelAddress > vmBlockCacheEndAddr) {
    return(SYS_INVALID_ARG);
  }
#endif

  numPages = ((kernelAddress + bytesLeft - 1) >> VMMACH_PAGE_SHIFT) - 
    (kernelAddress >> VMMACH_PAGE_SHIFT);
  if (!Dbg_InRange(kernelAddress, bytesLeft, FALSE)) {
    replyPtr->length = 0;
    return(SYS_ARG_NOACCESS);
  }
  bytesInPage = vm_PageSize - (kernelAddress & (vm_PageSize-1));
  if (bytesLeft < bytesInPage) {
    bcopy(kernelAddress, bufPtr, bytesLeft);
    replyPtr->length = readPtr->length;
    return(SUCCESS);
  }
  bcopy(kernelAddress, bufPtr, bytesInPage);
  bytesLeft -= bytesInPage;
  bufPtr += vm_PageSize;
  while (numPages > 0) {
    if (!Dbg_InRange(kernelAddress, bytesLeft, FALSE)) {
      replyPtr->length = 0;
      return(SYS_ARG_NOACCESS);
    }
    if (bytesLeft < vm_PageSize) {
      bcopy(kernelAddress, bufPtr, bytesLeft);
      replyPtr->length = readPtr->length;
      return(SUCCESS);
    }
    bcopy(kernelAddress, bufPtr, vm_PageSize);
    bytesLeft -= vm_PageSize;
    bufPtr += vm_PageSize;
    numPages--;
  }
  replyPtr->length = readPtr->length;
  return(SUCCESS);
}


/*
 *----------------------------------------------------------------------
 *
 *  Dev_SmemWrite --
 *
 *	Writes if it can, and returns SUCCESS if it wrote.
 *
 * Results:
 *	A standard Sprite return status.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
Dev_SmemWrite(devicePtr, writePtr, replyPtr)
    Fs_Device *devicePtr;
    Fs_IOParam	*writePtr;	/* Write parameter block */
    Fs_IOReply	*replyPtr;	/* Return length and signal */ 
{
  int bytesLeft;
  int kernelAddress;
  int numPages, bytesInPage;
  char *bufPtr;
  StopInfo stopInfo;
  Dbg_DumpBounds currentBounds;
  
  stopInfo.codeStart = (int)mach_CodeStart;
  stopInfo.trapType = 0;
/*  stopInfo.regs = ;*/
  currentBounds.pageSize = vm_PageSize;
  currentBounds.stackSize = mach_KernStackSize;
  currentBounds.kernelCodeStart = (unsigned int) mach_KernStart;
  currentBounds.kernelCodeSize  =
    (unsigned int) (((Address)(&end)) - mach_KernStart);
  currentBounds.kernelDataStart  = ((unsigned int)(&end));
  currentBounds.kernelDataSize   = (unsigned int)
    (((Address)(vmMemEnd)) - ((Address)(&end)));
  currentBounds.kernelStacksStart = (unsigned int)vmStackBaseAddr;
  currentBounds.kernelStacksSize = (unsigned int)
    (vmStackEndAddr - vmStackBaseAddr);
  currentBounds.fileCacheStart   = (unsigned int)vmBlockCacheBaseAddr;
  currentBounds.fileCacheSize    = (unsigned int) (vmBlockCacheEndAddr -
					    vmBlockCacheBaseAddr);
  kernelAddress = writePtr->offset;
  bytesLeft = writePtr->length;
  bufPtr = writePtr->buffer;
  if (kernelAddress < sizeof(StopInfo)) {
    if (bytesLeft + kernelAddress <= sizeof(StopInfo)) {
      bcopy(writePtr->buffer, ((char *)&stopInfo) + kernelAddress, writePtr->length);
      replyPtr->length = writePtr->length;
      return(SUCCESS);
    }
    bcopy(writePtr->buffer, ((char *)&stopInfo) + kernelAddress, sizeof(StopInfo) - kernelAddress);
    bytesLeft -= sizeof(StopInfo) - kernelAddress;
    bufPtr += sizeof(StopInfo) - kernelAddress;
    kernelAddress = sizeof(StopInfo);
  }
  if (kernelAddress < (sizeof(StopInfo) + sizeof(Dbg_DumpBounds))) {
    if (bytesLeft + kernelAddress <= sizeof(StopInfo) + sizeof(Dbg_DumpBounds)) {
      bcopy(bufPtr, ((char *)&currentBounds) + sizeof(StopInfo) - kernelAddress, bytesLeft);
      replyPtr->length = writePtr->length;
      return(SUCCESS);
    }
    bcopy(bufPtr, ((char *)&currentBounds) + sizeof(StopInfo) - kernelAddress, sizeof(Dbg_DumpBounds));
    bytesLeft -= sizeof(StopInfo) + sizeof(Dbg_DumpBounds) - kernelAddress;
    bufPtr += sizeof(StopInfo) + sizeof(Dbg_DumpBounds) - kernelAddress;
    kernelAddress = mach_KernStart;
  }
  kernelAddress += mach_KernStart - (sizeof(StopInfo) + sizeof(Dbg_DumpBounds));

#ifdef NOGAP
  if (kernelAddress > vmMemEnd) {
    kernelAddress += vmStackBaseAddr - vmMemEnd;
  }
  if (kernelAddress > vmStackEndAddr) {
    kernelAddress += vmBlockCacheBaseAddr - vmStackEndAddr;
  }
  if (kernelAddress > vmBlockCacheEndAddr) {
    return (SYS_INVALID_ARG);
  }
#else
  if (kernelAddress > vmMemEnd &&
      kernelAddress < vmStackBaseAddr) {
    return(SYS_INVALID_ARG);
  }
  if (kernelAddress > vmStackEndAddr &&
      kernelAddress < vmBlockCacheBaseAddr) {
    return(SYS_INVALID_ARG);
  }
  if (kernelAddress > vmBlockCacheEndAddr) {
    return(SYS_INVALID_ARG);
  }
#endif

  numPages = ((kernelAddress + bytesLeft - 1) >> VMMACH_PAGE_SHIFT) - 
    (kernelAddress >> VMMACH_PAGE_SHIFT);
  if (!Dbg_InRange(kernelAddress, bytesLeft, FALSE)) {
    replyPtr->length = 0;
    return(SYS_ARG_NOACCESS);
  }
  bytesInPage = vm_PageSize - (kernelAddress & (vm_PageSize-1));
  if (bytesLeft < bytesInPage) {
    bcopy(bufPtr, kernelAddress, bytesLeft);
    replyPtr->length = writePtr->length;
    return(SUCCESS);
  }
  bcopy(bufPtr, kernelAddress, bytesInPage);
  bytesLeft -= bytesInPage;
  bufPtr += vm_PageSize;
  while (numPages > 0) {
    if (!Dbg_InRange(kernelAddress, bytesLeft, FALSE)) {
      replyPtr->length = 0;
      return(SYS_ARG_NOACCESS);
    }
    if (bytesLeft < vm_PageSize) {
      bcopy(bufPtr, kernelAddress, bytesLeft);
      replyPtr->length = writePtr->length;
      return(SUCCESS);
    }
    bcopy(bufPtr, kernelAddress, vm_PageSize);
    bytesLeft -= vm_PageSize;
    bufPtr += vm_PageSize;
    numPages--;
  }
  replyPtr->length = writePtr->length;
  return(SUCCESS);
}


/*
 *----------------------------------------------------------------------
 *
 * Dev_SmemIOControl --
 *
 *	This procedure handles IOControls for /dev/smem and other
 *	devices.  It refuses all IOControls except for a few of
 *	the generic ones, for which it does nothing.
 *
 * Results:
 *	A standard Sprite return status.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
ReturnStatus
Dev_SmemIOControl(devicePtr, ioctlPtr, replyPtr)
    Fs_Device	        *devicePtr;
    Fs_IOCParam		*ioctlPtr;
    Fs_IOReply		*replyPtr;
{
    if ((ioctlPtr->command == IOC_GET_FLAGS)
	|| (ioctlPtr->command == IOC_SET_FLAGS)
	|| (ioctlPtr->command == IOC_SET_BITS)
	|| (ioctlPtr->command == IOC_CLEAR_BITS)
	|| (ioctlPtr->command == IOC_REPOSITION)) {
	return SUCCESS;
    }
    return GEN_NOT_IMPLEMENTED;
}

/*
 *----------------------------------------------------------------------
 *
 * Dev_SmemSelect --
 *
 *	This procedure handles selects for /dev/smem and other
 *	devices that are always ready.
 *
 * Results:
 *	The device is indicated to be readable and writable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
ReturnStatus
Dev_SmemSelect(devicePtr, readPtr, writePtr, exceptPtr)
    Fs_Device	*devicePtr;	/* Ignored. */
    int	*readPtr;		/* Read bit to clear if not readable */
    int	*writePtr;		/* Write bit to clear if not readable */
    int	*exceptPtr;		/* Except bit to clear if not readable */
{
    *exceptPtr = 0;
    return(SUCCESS);
}
