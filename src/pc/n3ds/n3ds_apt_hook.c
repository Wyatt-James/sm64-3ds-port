#include "n3ds_apt_hook.h"

#include <stdio.h>

#include "macros.h"
#include "src/pc/n3ds/n3ds_threading.h"
#include "src/pc/n3ds/n3ds_config.h"
#include "src/pc/audio/audio_3ds.h"

bool n3ds_apt_suspended;

static aptHookCookie apt_hook_cookie;
static bool hooked = false;
static uint32_t appSuspendCounter = 0; // Greater than 0 when the 3DS lid is closed or home button is pressed

// Called whenever a 3DS OS event is fired. Runs synchronously on thread5.
static void apt_hook_function(APT_HookType hook, UNUSED void* param)
{
    if (!hooked)
        return;

    char* eventName = "unknown";

    switch (hook) {
        case APTHOOK_ONSLEEP: // Lid closed
            eventName = "sleep";
            appSuspendCounter++;
            break;

        case APTHOOK_ONSUSPEND: // Home menu opened
            eventName = "suspend";
            appSuspendCounter++;
            break;

        case APTHOOK_ONWAKEUP: // Lid opened
            eventName = "wake-up";
            appSuspendCounter--;
            break;

        case APTHOOK_ONRESTORE: // Home menu closed
            eventName = "restore";
            appSuspendCounter--;
            break;

        case APTHOOK_ONEXIT: // Application exit
            eventName = "exit";
            g3dsConfig.run = false;
            break;
        
        case APTHOOK_COUNT: // Unused - should never happen
            fprintf(stderr, "Invalid APT hook type: count.\n");
            return;
            
        default: // Should never happen
            fprintf(stderr, "Unknown APT hook type %d.\n", hook);
            return;
    }

    printf("AptHook caught: %s.\n", eventName);

    // Mute audio when sleeping, unmute when waking
    const float vol = appSuspendCounter > 0 ? 0.0f : 1.0f;
    printf("Setting NDSP volume to: %f\n", vol);
    audio_3ds_set_dsp_volume(vol, vol);

    // Lower CPU priority only if applicable
    if (n3ds_old_core_1_is_available) {
        const __3ds_u32 limit = appSuspendCounter > 0 ? g3dsConfig.syscore_limit_idle : g3dsConfig.syscore_limit;

        if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(limit)))
            printf("AppCpuTimeLimit set to %ld.\n", limit);
        else
            fprintf(stderr, "Error: AppCpuTimeLimit failed to set to %ld.\n", limit);
    } else {
        printf("Not altering speed of disabled OLD_CORE_1.\n");
    }

    n3ds_apt_suspended = appSuspendCounter > 0;
}

static void reset_counters()
{
    n3ds_apt_suspended = false;
    appSuspendCounter = 0;
}

void n3ds_apt_hook_init(void)
{
    aptHook(&apt_hook_cookie, apt_hook_function, NULL);
    hooked = true;
    reset_counters();
}

void n3ds_apt_hook_exit(void)
{
    aptUnhook(&apt_hook_cookie);
    hooked = false;
    reset_counters();
}
