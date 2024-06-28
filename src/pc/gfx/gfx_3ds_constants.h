#ifndef GFX_3DS_CONSTANTS_H
#define GFX_3DS_CONSTANTS_H

enum Stereoscopic3dMode {
    STEREO_MODE_3D               = 0, // 0: 3D
    STEREO_MODE_2D,                   // 1: Pure 2D
    STEREO_MODE_3D_GODDARD_HAND,      // 2: Goddard hand and press start text
    STEREO_MODE_3D_CREDITS,           // 3: Credits
    STEREO_MODE_3D_SCORE_MENU,        // 4: The goddamned score menu
    STEREO_MODE_COUNT                 // 5: Number of modes
};

#endif
