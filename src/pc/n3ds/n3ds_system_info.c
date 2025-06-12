#include "n3ds_system_info.h"

N3DS_SystemInfo g3dsSystemInfo;

static bool is_new_n3ds()
{
    bool is_new_n3ds = false;
    return R_SUCCEEDED(APT_CheckNew3DS(&is_new_n3ds)) ? is_new_n3ds : false;
}

static CFG_SystemModel get_system_model()
{
    if (R_SUCCEEDED(cfguInit()))
    {
        __3ds_u8 model;

        if (R_SUCCEEDED(CFGU_GetSystemModel(&model)))
            return (CFG_SystemModel) model;
        else
            return CFG_MODEL_2DS; // Most limited hardware

        cfguExit();
    }
    else
    {
        return CFG_MODEL_2DS; // Most limited hardware
    }
}

void n3ds_system_info_init(void)
{
    N3DS_SystemInfo* info = &g3dsSystemInfo;
    info->is_new_3ds = is_new_n3ds();
    info->hardware_version = get_system_model();
    info->supports_800px = info->hardware_version != CFG_MODEL_2DS;
    info->initialized = true;
}
