#pragma once

/*
 * A convenient API for getting N3DS system-specific information.
 */

#include "libctru_inc.h"

typedef struct
{
    bool initialized,                 // Set to true when this system info is initialized
         is_new_3ds,                  // True if running on a New 3DS
         supports_800px;              // True if this console supports 800px width mode (old 2DS does not)
    CFG_SystemModel hardware_version; // The specific hardware revision of this console
} N3DS_SystemInfo;

extern N3DS_SystemInfo g3dsSystemInfo;

// Initializes the system info flags
extern void n3ds_system_info_init(void);
