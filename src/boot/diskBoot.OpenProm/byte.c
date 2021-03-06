/*
 *----------------------------------------------------------------------
 * 
 * bcopy --
 *
 *	Copy numBytes from *sourcePtr to *destPtr.  This routine is
 *	optimized to do transfers when sourcePtr and destPtr are both
 *	integer-aligned and point to large areas.
 *
 * Results:
 *	There is no return value.  The memory at *destPtr is modified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

bcopy(sourcePtr, destPtr, numBytes)
    register char *sourcePtr;		/* Where to copy from */
    register char *destPtr;		/* Where to copy to */
    register int numBytes;	/* The number of bytes to copy */
{
    while (numBytes-- > 0) {
	*(destPtr++) = *(sourcePtr++);
    }
}
bzero( destPtr, numBytes)
    register char *destPtr;		/* Where to zero to */
    register int numBytes;	/* The number of bytes to zero */
{
    while (numBytes-- > 0) {
	*(destPtr++) = 0;
    }
}

strcmp(s1, s2)
     register char *s1, *s2;
{
     while (1) {
	if (*s1 != *s2) {
	    if (*s1 > *s2) {
		return 1;
	    } else {
		return -1;
	    }
	}
	if (*s1++ == 0) {
	    return 0;
	}
	s2 += 1;
    }

}
