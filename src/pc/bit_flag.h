#pragma once

/*
 * A file of bitflag macros.
 */

#define FLAG_ON(flags_, flag_)    ((flags_) &   (flag_)) // Returns which flags are set
#define FLAG_SET(flags_, flag_)   ((flags_) |=  (flag_)) // Sets the given flags
#define FLAG_CLEAR(flags_, flag_)   flags_  &= ~(flag_)  // Clears the given flags
#define FLAG_ALL (~0)                                    // All flags on
