#include "n3ds_system_info.h"

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
#include <3ds/result.h>       // For R_SUCCEEDED
#include <3ds/services/apt.h> // For APT_CheckNew3ds
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

// Forward declarations
static bool is_new_n3ds();
static void get_cfgu_info();

bool n3ds_system_info_is_initialized = false;

CFG_SystemModel n3ds_hardware_version = CFG_MODEL_3DS;
bool n3ds_supports_800px_mode = false;
bool n3ds_is_new_3ds = false;

void n3ds_init_system_info()
{
    n3ds_is_new_3ds = is_new_n3ds();
    get_cfgu_info();
    n3ds_supports_800px_mode = n3ds_hardware_version != CFG_MODEL_2DS;
    n3ds_system_info_is_initialized = true;
}

static bool is_new_n3ds()
{
    bool is_new_n3ds = false;
    return R_SUCCEEDED(APT_CheckNew3DS(&is_new_n3ds)) ? is_new_n3ds : false;
}

static void get_cfgu_info()
{
    if (R_SUCCEEDED(cfguInit()))
    {
        __3ds_u8 model;

        if (R_SUCCEEDED(CFGU_GetSystemModel(&model)))
            n3ds_hardware_version = (CFG_SystemModel) model;
        else
            n3ds_hardware_version = CFG_MODEL_3DS;

        cfguExit();
    }
}
