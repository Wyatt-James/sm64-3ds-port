#pragma once

/*
 * A nice little wrapper for N3DS HID, including the 3D slider.
 */

#include <stdint.h>
#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"

typedef enum
{
    N3DS_INPUT_START_OF_FRAME,  // Input is polled at the start of the frame. Best consistency, worst latency.
    N3DS_INPUT_DEFERRED_SINGLE, // Input is polled the first time it's used. Good if you need identical latency for all input types, but inconsistent frame times cause jitter.
    N3DS_INPUT_DEFERRED_MULTI,  // Input is polled each time it's used each frame, per-type. Good latency, but inconsistent frame times cause jitter.
    N3DS_INPUT_ALWAYS,          // Input is always polled. Minimizes latency, but bad for couch co-op, and some behavior is undefined.
} N3DS_HidPollMode;

typedef struct
{
    touchPosition position;
    uint32_t frames_held;
} N3DS_TouchState;

typedef struct
{
    circlePosition circle_pad, c_stick;
    __3ds_u32 held;
} N3DS_ButtonState;

void              n3ds_hid_start_frame(void);    // Call this at the start of your frame.
N3DS_TouchState*  n3ds_hid_touch(void);          // Returns the current touch state.
N3DS_ButtonState* n3ds_hid_buttons(void);        // Returns the current button & stick state.
float             n3ds_hid_3d_slider(void);      // Returns the current 3D slider state.

N3DS_TouchState*  n3ds_hid_prev_touch(void);     // Returns the previous frame's touch state.
N3DS_ButtonState* n3ds_hid_prev_buttons(void);   // Returns the previous frame's button & stick state.
float             n3ds_hid_prev_3d_slider(void); // Returns the previous frame's 3D slider state.
