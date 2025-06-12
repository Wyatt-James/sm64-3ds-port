#pragma once

/*
 * A system for creating and rendering click/touch menus.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define CLICK_FLAGS_ALL  (~(Click_Flags) 0)
#define CLICK_FLAGS_NONE ( (Click_Flags) 0)

typedef uint16_t Click_Flags;

typedef enum
{
    CLICK_ACTION_PRESS     = 0b00001, // Processed when the user first presses down onto the touch screen, and lands on a button.
    CLICK_ACTION_SLIDE_ON  = 0b00010, // Processed when a button is 'slid onto' but the screen was not just pressed.
    CLICK_ACTION_HOLD      = 0b00100, // Processed every frame a button is held
    CLICK_ACTION_SLIDE_OFF = 0b01000, // Processed when a button is 'slid off of' but the menu was not just fully released.
    CLICK_ACTION_RELEASE   = 0b10000, // Processed when a button is released and the menu is as well.
    CLICK_ACTION_ALL       = (CLICK_ACTION_RELEASE << 1) - 1,
} Click_Action;

typedef enum
{
    CLICK_NO_CAPTURE, // Does not capture clicks, i.e. can be clicked through
    CLICK_CAPTURE,    // Cannot be clicked through
} Click_CaptureMode;

typedef union
{
    struct {
        int x, y, w, h;
    };
    struct {
        int left, top, right, bottom;
    };
} Click_Region;

typedef union
{
    uint32_t u32;
    uintptr_t int_ptr;
    void* raw_ptr;
} Click_CustomData;

typedef struct
{
    Click_Region padded_region;
} Click_InternalData;

typedef struct Click_Button_tag Click_Button;

struct Click_Button_tag
{
    Click_Region position;          // Actual location of the button
    Click_Region click_padding;     // How much larger the button's active region should be than its position shows.
    Click_CaptureMode capture_mode; // Defines the behavior of click-through.
    Click_CustomData custom_data;   // Holds either embedded data or a pointer to data.
    Click_Flags render_groups;      // Bitflags for which render groups groups this button belongs to.
    Click_Flags click_groups;       // Bitflags for which click groups this button belongs to.
    void* graphic;                  // Platform-specific graphics data.
    void (*on_press)(Click_Button* self);
    void (*on_hold)(Click_Button* self);
    void (*on_slide_onto)(Click_Button* self);
    void (*on_slide_off)(Click_Button* self);
    void (*on_release)(Click_Button* self);
    void (*render)(Click_Button* self, void* common_data);
    bool currently_held;
    Click_InternalData internal;    // Don't touch me!
    char* name;
};

typedef struct {
    int x, y;
} Click_Point;

/* 
 * Evaluates the given buttons.
 * point is the touched point.
 * The flags bitfield is used to filter which buttons are processed.
 * The actions bitfield is used to filter which actions are processed.
 * print_interactions enables debug printing of each processed interaction/button.
 */
void click_evaluate(size_t button_count, Click_Button buttons[button_count], Click_Point point, Click_Flags flags, Click_Action actions, bool print_interactions);

/* 
 * Evaluates the given buttons, but treats all buttons as capturing.
 * point is the touched point.
 * The flags bitfield is used to filter which buttons are processed.
 * The actions bitfield is used to filter which actions are processed.
 * print_interactions enables debug printing of each processed interaction/button.
 */
void click_evaluate_single(size_t button_count, Click_Button buttons[button_count], Click_Point point, Click_Flags flags, Click_Action actions, bool print_interactions);

// Renders an array of buttons with the given common data.
void click_render(size_t button_count, Click_Button buttons[button_count], Click_Flags flags, void* common_data);

// Initializes multiple buttons.
void click_init(size_t button_count, Click_Button buttons[button_count]);

// Initializes a single button.
void click_init_single(Click_Button buttons[1]);
