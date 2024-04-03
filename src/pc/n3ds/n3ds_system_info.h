#ifndef N3DS_SYSTEM_INFO_H
#define N3DS_SYSTEM_INFO_H

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <stdbool.h>           // Used for bool typedef
#include <3ds/services/cfgu.h> // Used for CFG_SystemModel
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

extern bool n3ds_system_info_is_initialized;

extern CFG_SystemModel n3ds_hardware_version;
extern bool n3ds_supports_800px_mode;
extern bool n3ds_is_new_3ds;

// Initializes the system info flags
extern void n3ds_init_system_info();

#endif
