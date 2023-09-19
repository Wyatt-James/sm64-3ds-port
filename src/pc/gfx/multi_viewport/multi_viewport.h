#ifndef MULTI_VIEWPORT_H
#define MULTI_VIEWPORT_H

// Data that is generally specified per-viewport

#define VIEWPORT_SHOULD_CLEAR(MODE) (MODE & 0b01)

// Determines when a display should be cleared automatically by the graphics API.
// Bit 0 signifies clear.
enum ViewportClearMode {
    VIEW_CLEAR_OFF       = 0,   // 00: The viewport will not be cleared
    VIEW_CLEAR_ON        = 1,   // 01: The viewport will be cleared before every frame
    VIEW_CLEAR_OFF_ONCE  = 2,   // 10: The viewport will be cleared repeatedly, after the next frame
    VIEW_CLEAR_ON_ONCE   = 3    // 11: The viewport will be cleared once, before the next frame
};

// Updates the value of a ViewportClearMode.
void updateClearMode(enum ViewportClearMode* e);

#endif