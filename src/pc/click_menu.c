#include "click_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define WITHIN(c_, min_, width_) (((c_) > (min_)) && ((c_) < (min_) + (width_)))

static inline bool is_inside_region(Click_Point point, Click_Region region)
{
    return WITHIN(point.x, region.x, region.w)
        && WITHIN(point.y, region.y, region.h);
}

static inline Click_Region region_add(Click_Region r1, Click_Region r2)
{
    return (Click_Region) {
        .x = r1.x + r2.x,
        .y = r1.y + r2.y,
        .w = r1.w + r2.w,
        .h = r1.h + r2.h
    };
}

// Expands a region on each side by the specified amount
static inline Click_Region region_expand(Click_Region r1, Click_Region r2)
{
    return (Click_Region) {
        .x = r1.x - r2.left,
        .y = r1.y - r2.top,
        .w = r1.w + r2.left + r2.right,
        .h = r1.h + r2.top  + r2.bottom
    };
}

static void print_interaction(Click_Button* button, char* interaction_name, bool do_print)
{
    if (!do_print)
        return;

    if(button->name != NULL)
    {
        printf("Button %s %s\n", button->name, interaction_name);
    }
    else
    {
        printf("Unnamed Button %s\n", interaction_name);
    }
}

static void click_evaluate_internal(Click_Point point, Click_Flags flags, size_t button_count, Click_Button buttons[button_count], bool evaluate_single, Click_Action actions, bool print_interactions)
{
    const bool process_presses    = (actions & CLICK_ACTION_PRESS),
               process_holds      = (actions & CLICK_ACTION_HOLD),
               process_slide_on   = (actions & CLICK_ACTION_SLIDE_ON),
               process_slide_off  = (actions & CLICK_ACTION_SLIDE_OFF),
               process_releases   = (actions & CLICK_ACTION_RELEASE);

    bool captured = false;

    for (size_t i = 0; i < button_count; i++)
    {
        Click_Button* button = &buttons[i];

        // Ignore buttons outside our current group
        if (!(flags & button->click_groups))
            continue;

        if (!captured && is_inside_region(point, button->internal.padded_region))
        {
            bool already_held = button->currently_held;
            button->currently_held = true;
            
            if (process_presses && !already_held && button->on_press != NULL)
            {
                print_interaction(button, "Pressed", print_interactions);
                button->on_press(button);
            }
            
            if (process_slide_on && !already_held && button->on_slide_onto != NULL)
            {
                print_interaction(button, "Slid Onto", print_interactions);
                button->on_slide_onto(button);
            }

            if (process_holds && button->on_hold != NULL)
            {
                print_interaction(button, "Held", print_interactions);
                button->on_hold(button);
            }

            if(button->capture_mode == CLICK_CAPTURE || evaluate_single)
                captured = true;
        }
        else // If we've been captured, or if this button isn't being pressed, evaluate releases
        {
            if (button->currently_held)
            {
                button->currently_held = false;

                if (process_releases && button->on_release != NULL)
                {
                    print_interaction(button, "Released", print_interactions);
                    button->on_release(button);
                }
            
                if (process_slide_off && button->on_slide_off != NULL)
                {
                    print_interaction(button, "Slid Off Of", print_interactions);
                    button->on_slide_off(button);
                }
    
            }
        }
    }
}

void click_evaluate(size_t button_count, Click_Button buttons[button_count], Click_Point point, Click_Flags flags, Click_Action actions, bool print_interactions)
{
    return click_evaluate_internal(point, flags, button_count, buttons, false, actions, print_interactions);
}

void click_evaluate_single(size_t button_count, Click_Button buttons[button_count], Click_Point point, Click_Flags flags, Click_Action actions, bool print_interactions)
{
    return click_evaluate_internal(point, flags, button_count, buttons, true, actions, print_interactions);
}

void click_render(size_t button_count, Click_Button buttons[button_count], Click_Flags flags, void* common_data)
{
    // Render in reverse priority order
    for (size_t i = 0; i < button_count; i++)
    {
        Click_Button* button = &buttons[button_count - i - 1];
        if ((button->render_groups & flags) && button->render != NULL)
        {
            button->render(button, common_data);
        }
    }
}

void click_init(size_t button_count, Click_Button buttons[button_count])
{
    for (size_t i = 0; i < button_count; i++)
    {
        click_init_single(&buttons[i]);
    }
}

void click_init_single(Click_Button button[1])
{
    button->currently_held = false;
    button->internal.padded_region = region_expand(button->position, button->click_padding);
}
