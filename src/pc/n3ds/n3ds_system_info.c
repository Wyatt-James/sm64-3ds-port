#include "n3ds_system_info.h"

#include <stdio.h>

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

static const char* get_system_model_string(CFG_SystemModel model)
{
    switch (model) {
        case CFG_MODEL_3DS:    return "3DS"; // Old 3DS (CTR)
        case CFG_MODEL_3DSXL:  return "3DS XL"; // Old 3DS XL (SPR)
        case CFG_MODEL_N3DS:   return "New 3DS"; // New 3DS (KTR)
        case CFG_MODEL_2DS:    return "2DS"; // Old 2DS (FTR)
        case CFG_MODEL_N3DSXL: return "New 3DS XL"; // New 3DS XL (RED)
        case CFG_MODEL_N2DSXL: return "New 2DS XL"; // New 2DS XL (JAN)
        default:               return "Unknown";
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

void n3ds_system_info_print(void)
{
    N3DS_SystemInfo* info = &g3dsSystemInfo;
    if (!info->initialized) {
        printf("Warn: n3ds_system_info_print called without prior initialization.\n");
        return;
    }

    printf("System model: %s\n", get_system_model_string(g3dsSystemInfo.hardware_version));
    printf("New 3DS features enabled: %c\n", info->is_new_3ds ? 'Y' : 'N');
    printf("Supports 800px: %c\n", g3dsSystemInfo.supports_800px ? 'Y' : 'N');
}
