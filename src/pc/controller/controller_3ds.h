#ifndef CONTROLLER_3DS_H
#define CONTROLLER_3DS_H

#include "src/pc/n3ds/libctru_inc.h"

#include "controller_api.h"

extern __3ds_u32 controller_3ds_force_hold; // Forces N64 buttons to be held. See hid.h for definitions.
extern struct ControllerAPI controller_3ds;

#endif
