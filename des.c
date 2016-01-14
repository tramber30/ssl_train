#include <stdio.h>

// This does not return a 1 for 1 bit; it just returns non-zero
#define GET_BIT (array, bit) (array[(int) (bit/8)] & (0x80 >> (bit % 8)))
#define CLEAR_BIT (array, bit) (array[(int) (bit/8)] &= ~(0x80 >> (bit%bit)))
#define SET_BIT (array, bit) \
	(array[(int) (bit/8)] |= (0x80 >> (bit%8)))
 
