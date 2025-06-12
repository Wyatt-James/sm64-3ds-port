#ifdef TARGET_N3DS

#include "src/pc/n3ds/libctru_inc.h"

#include <ultra64.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "controller_api.h"

#include "../configfile.h"
#include "src/pc/n3ds/n3ds_hid.h"

typedef struct
{
    u16 n64;
    __3ds_u32 n3ds;
} ButtonMapping;

u16 controller_3ds_force_hold = 0;
static ButtonMapping button_mapping[10];

static void set_button_mapping(int index, u16 mask_n64, unsigned int mask_3ds)
{
    button_mapping[index].n3ds = (__3ds_u32) mask_3ds;
    button_mapping[index].n64 = mask_n64;
}

static u16 controller_3ds_get_held(void)
{
    u16 res = 0;
    N3DS_ButtonState* buttons = n3ds_hid_buttons();
    __3ds_u32 kHeld = buttons->held;

    for (size_t i = 0; i < sizeof(button_mapping) / sizeof(button_mapping[0]); i++)
    {
        if (button_mapping[i].n3ds & kHeld) {
            res |= button_mapping[i].n64;
        }
    }

    res |= controller_3ds_force_hold;

    return res;
}

static void controller_3ds_init(void)
{
    u32 i = 0;
    set_button_mapping(i++, A_BUTTON,     configKeyA); // n64 button => configured button
    set_button_mapping(i++, B_BUTTON,     configKeyB);
    set_button_mapping(i++, START_BUTTON, configKeyStart);
    set_button_mapping(i++, L_TRIG,       configKeyL);
    set_button_mapping(i++, R_TRIG,       configKeyR);
    set_button_mapping(i++, Z_TRIG,       configKeyZ);
    set_button_mapping(i++, U_CBUTTONS,   configKeyCUp);
    set_button_mapping(i++, D_CBUTTONS,   configKeyCDown);
    set_button_mapping(i++, L_CBUTTONS,   configKeyCLeft);
    set_button_mapping(i++, R_CBUTTONS,   configKeyCRight);
}

static void controller_3ds_read(OSContPad *pad)
{
    pad->button = controller_3ds_get_held();

    circlePosition* circle_pad = &n3ds_hid_buttons()->circle_pad;
    pad->stick_x = circle_pad->dx / 2;
    pad->stick_y = circle_pad->dy / 2;
}

struct ControllerAPI controller_3ds = {
    controller_3ds_init,
    controller_3ds_read
};

#endif
