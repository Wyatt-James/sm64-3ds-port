#ifndef GFX_3DS_CONSTANTS_H
#define GFX_3DS_CONSTANTS_H

enum Stereoscopic3dMode {
    STEREO_MODE_2D               = 0, // Pure 2D
    STEREO_MODE_3D,                   // 3D
    STEREO_MODE_3D_GODDARD_HAND,      // Goddard hand
    STEREO_MODE_3D_CREDITS,           // Credits
    STEREO_MODE_COUNT                 // Number of modes
};

enum IodMode {
    IOD_NORMAL       =  0,
    IOD_GODDARD,     // 1
    IOD_FILE_SELECT, // 2
    IOD_STAR_SELECT, // 3
    IOD_CANNON,      // 4
    IOD_COUNT        // Number of modes
};

#endif
