#ifndef BIT_FLAG_H
#define BIT_FLAG_H

/*
 * A file of bitflag macros.
 */

#define SET_BITS(num_bits_) ((1 << (num_bits_)) - 1) // Sets N least significant bits. For example, (2) = 0b11.
#define GET_BITS(src_, lower_, upper_)  ((src_ >> lower_) & SET_BITS(upper_ - lower_ + 1)) // Gets the requested bits, inclusive, from the source.
#define GET_BITS2(src_, lower_, count_) ((src_ >> lower_) & SET_BITS(count_)) // Gets the requested bits from the source.

// State accessors and expressions

#define FLAG_ON(flags_, flag_)      ((flags_) &  (flag_)) // Returns which flags are set
#define FLAG_WITH(flags_, flag_)    ((flags_) |  (flag_)) // Sets the given flags
#define FLAG_WITHOUT(flags_, flag_) ((flags_) & ~(flag_)) // Unsets the given flags
#define FLAG_ALL (~0)                                     // All flags on
#define FLAG_NONE (0)                                     // All flags off

// Statements

#define FLAG_SET(flags_, flag_)   flags_ = FLAG_WITH(flags_, flag_)    // Sets the given flags
#define FLAG_CLEAR(flags_, flag_) flags_ = FLAG_WITHOUT(flags_, flag_) // Clears the given flags

#endif
