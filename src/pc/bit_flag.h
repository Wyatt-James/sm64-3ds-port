#ifndef BIT_FLAG_H
#define BIT_FLAG_H

/*
 * A file of bitflag macros.
 */

#define SET_BITS(num_bits_) ((1 << (num_bits_)) - 1)    // Sets N least significant bits. For example, (2) = 0b11.

#define FLAG_ON(flags_, flag_)    ((flags_) &  (flag_)) // Returns which flags are set
#define FLAG_SET(flags_, flag_)   ((flags_) |= (flag_)) // Sets the given flags
#define FLAG_CLEAR(flags_, flag_)  flags_ &= ~(flag_)   // Clears the given flags
#define FLAG_ALL (~0)                                   // All flags on

#endif
