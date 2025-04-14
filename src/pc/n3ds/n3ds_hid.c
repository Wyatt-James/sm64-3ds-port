#include "n3ds_hid.h"

#include <stdio.h>
#include "macros.h"
#include "n3ds_config.h"

typedef enum
{
    POLL_BUTTONS = BIT(0),                  // Buttons and sticks
    POLL_TOUCH   = BIT(1),                  // Touch screen
    POLL_SLIDER  = BIT(2),                  // 3D Slider
    POLL_ALL     = ((POLL_SLIDER << 1) - 1) // All categories
} PollCategory;

typedef struct
{
    N3DS_TouchState touch;
    N3DS_ButtonState buttons;
    float slider;

    float slider_level;
    PollCategory polled;
} HidState;

HidState prev_hid_state = {};
HidState hid_state = {};

static bool should_poll_mid_frame(PollCategory category)
{
    switch (g3dsConfig.input_poll_mode) {
        default:
        case N3DS_INPUT_START_OF_FRAME:  return false;
        case N3DS_INPUT_DEFERRED_SINGLE: return hid_state.polled ? false : true;
        case N3DS_INPUT_DEFERRED_MULTI:  return (hid_state.polled & category) ? false : true;
        case N3DS_INPUT_ALWAYS:          return true;
    }
}

// This polls ALL inputs, but only sets the given flags. This behaves this way
// to accommodate for N3DS_INPUT_DEFERRED_MULTI, which
static void poll_internal(PollCategory flags_to_set)
{
    hidScanInput();
    hid_state.slider_level = osGet3DSliderState();
    hid_state.polled |= flags_to_set;
}

static void try_poll_mid_frame(PollCategory category)
{
    if (should_poll_mid_frame(category))
        poll_internal(category);
}

void n3ds_hid_start_frame(void)
{
    prev_hid_state = hid_state; // Struct copy
    hid_state.polled = 0;
    
    if (g3dsConfig.input_poll_mode == N3DS_INPUT_START_OF_FRAME)
        poll_internal(POLL_ALL);
}

N3DS_TouchState* n3ds_hid_touch(void)
{
    try_poll_mid_frame(POLL_TOUCH);

    hidTouchRead(&hid_state.touch.position);

    bool touched = (hid_state.touch.position.px || hid_state.touch.position.py);
    
    if (touched)
        hid_state.touch.frames_held = prev_hid_state.touch.frames_held + 1;
    else
        hid_state.touch.frames_held = 0;
    
    return &hid_state.touch;
}

N3DS_ButtonState* n3ds_hid_buttons(void)
{
    try_poll_mid_frame(POLL_BUTTONS);

    hidCircleRead(&hid_state.buttons.circle_pad);
    hid_state.buttons.c_stick = (circlePosition) {}; // WYATT_TODO implement c-stick
    hid_state.buttons.held = hidKeysHeld();
    
    return &hid_state.buttons;
}

float n3ds_hid_3d_slider(void)
{
    try_poll_mid_frame(POLL_SLIDER);

    hid_state.slider = hid_state.slider_level;
    
    return hid_state.slider;
}

N3DS_TouchState* n3ds_hid_prev_touch(void)
{
    return &prev_hid_state.touch;
}

N3DS_ButtonState* n3ds_hid_prev_buttons(void)
{
    return &prev_hid_state.buttons;
}

float n3ds_hid_prev_3d_slider(void)
{
    return prev_hid_state.slider;
}

