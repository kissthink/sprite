/*
 * bitInt.h --
 *
 *	Definitions used internally by the bit routines, but not
 *	exported to the outside world.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: bitInt.h,v 1.1 88/06/19 14:34:53 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _BITINT
#define _BITINT

/*
 * The Bit_FindFirst* routines use a binary search algorithm. Compared with
 * a simple iteration algorithm, the algorithm used below is better because
 * 1) the average execution time is about 10% less.
 * 2) the execution time is relatively independent of the position of 
 *    the bit in the integer. Iteration excution time is proportional to
 *    the bit position.
 *
 * The two Bit_FindFirst* routines use the same basic algorithm. Two macros, 
 * QUICK_TEST and TEST have different definitions depending on the routine.
 *
 * The QUICK_TEST at the top of the loop tests "temp" to see if it needs to be
 * searched. If QUICK_TEST is true, then at least 1 bit is set/cleared in temp.
 * The basic algorithm is "see if that bit is in the right half otherwise it 
 * must be in the left half". 
 */

#define SET_QUICK_TEST		if (temp != 0) 
#define CLEAR_QUICK_TEST	if (temp != ~0) 

#define SET_TEST(mask)		if ((temp & (mask)) != 0) 
#define CLEAR_TEST(mask)	if ((temp & (mask)) != (mask)) 

/*
 * RETURN returns the index of the first bit that is set/cleared in the
 * intNum integer in the array.
 */
#define RETURN(num) 	return((intNum*BIT_NUM_BITS_PER_INT)+num);

#define FIND_FIRST(numBits, arrayPtr) { \
    register int numInts = Bit_NumInts(numBits); \
    register int temp, intNum; \
    for (intNum = 0; intNum < numInts; intNum++, arrayPtr++) { \
	temp = *arrayPtr; \
	QUICK_TEST { \
	    TEST(0x0000ffff) { \
		TEST(0x000000ff) { \
		    TEST(0x0000000f) { \
			TEST(0x00000003) { \
			    TEST(0x00000001) { \
				RETURN(0); \
			    } else { \
				RETURN(1); \
			    } \
			} else { \
			    TEST(0x00000004) { \
				RETURN(2); \
			    } else { \
				RETURN(3); \
			    } \
			} \
		    } else { \
			TEST(0x00000030) { \
			    TEST(0x00000010) { \
				RETURN(4); \
			    } else { \
				RETURN(5); \
			    } \
			} else { \
			    TEST(0x00000040) { \
				RETURN(6); \
			    } else { \
				RETURN(7); \
			    } \
			} \
		    } \
		} else { \
		    TEST(0x00000f00) { \
			TEST(0x00000300) { \
			    TEST(0x00000100) { \
				RETURN(8); \
			    } else { \
				RETURN(9); \
			    } \
			} else { \
			    TEST(0x00000400) { \
				RETURN(10); \
			    } else { \
				RETURN(11); \
			    } \
			} \
		    } else { \
			TEST(0x00003000) { \
			    TEST(0x00001000) { \
				RETURN(12); \
			    } else { \
				RETURN(13); \
			    } \
			} else { \
			    TEST(0x00004000) { \
				RETURN(14); \
			    } else { \
				RETURN(15); \
			    } \
			} \
		    } \
		} \
	    } else { \
		TEST(0x00ff0000) {  \
		    TEST(0x000f0000) { \
			TEST(0x00030000) { \
			    TEST(0x00010000) { \
				RETURN(16); \
			    } else { \
				RETURN(17); \
			    } \
			} else { \
			    TEST(0x00040000) { \
				RETURN(18); \
			    } else { \
				RETURN(19); \
			    } \
			} \
		    } else { \
			TEST(0x00300000) { \
			    TEST(0x00100000) { \
				RETURN(20); \
			    } else { \
				RETURN(21); \
			    } \
			} else { \
			    TEST(0x00400000) { \
				RETURN(22); \
			    } else { \
				RETURN(23); \
			    } \
			} \
		    } \
		} else { \
		    TEST(0x0f000000) { \
			TEST(0x03000000) { \
			    TEST(0x01000000) { \
				RETURN(24); \
			    } else { \
				RETURN(25); \
			    } \
			} else { \
			    TEST(0x04000000) { \
				RETURN(26); \
			    } else { \
				RETURN(27); \
			    } \
			} \
		    } else { \
			TEST(0x30000000) { \
			    TEST(0x10000000) { \
				RETURN(28); \
			    } else { \
				RETURN(29); \
			    } \
			} else { \
			    TEST(0x40000000) { \
				RETURN(30); \
			    } else { \
				RETURN(31); \
			    } \
			} \
		    } \
		} \
	    } \
	} \
    } \
    return(-1); \
}

#define GET_MASK_AND_INTS(nB,i,m) \
 	i = (nB) / BIT_NUM_BITS_PER_INT; \
	m = ((1 << ((nB) % BIT_NUM_BITS_PER_INT)) - 1)

#endif _BITINT
