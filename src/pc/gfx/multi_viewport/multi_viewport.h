#ifndef MULTI_VIEWPORT_H
#define MULTI_VIEWPORT_H

// Data that is generally specified per-viewport

// Determines when a display should be cleared automatically by the graphics API.
enum ViewportClearMode {
    VIEW_CLEAR_OFF    = 0,      // 0: The screen will not be cleared
    VIEW_CLEAR_ON,              // 1: The screen will be cleared before every frame
    VIEW_CLEAR_ONCE             // 2: The screen will be cleared before the next frame
};

// Updates the value of a ViewportClearMode.
void updateClearMode(enum ViewportClearMode* e);

#endif