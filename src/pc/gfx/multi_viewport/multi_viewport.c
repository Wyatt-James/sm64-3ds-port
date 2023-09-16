#include "multi_viewport.h"

// Defines the next state for each ViewportClearMode
enum ViewportClearMode viewport_next_states[] = {
    VIEW_CLEAR_OFF,    // OFF stays off
    VIEW_CLEAR_ON,     // ON stays on
    VIEW_CLEAR_OFF     // ONCE becomes OFF
};

// Updates the value of a ViewportClearMode.
void updateClearMode(enum ViewportClearMode* e) {
    *e = viewport_next_states[*e];
}