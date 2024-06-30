#ifndef GFX_3DS_CONSTANTS_H
#define GFX_3DS_CONSTANTS_H

enum Stereoscopic3dMode {
    STEREO_MODE_2D               = 0, // Pure 2D
    STEREO_MODE_3D,                   // 3D
    STEREO_MODE_3D_GODDARD_HAND,      // Goddard hand and press start text
    STEREO_MODE_3D_CREDITS,           // Credits
    STEREO_MODE_COUNT                 // Number of modes
};

#endif
