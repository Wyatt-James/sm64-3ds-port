# Super Mario 64 Nintendo 3DS Port Controller Remapping Guide

This document describes how to remap controls for SM643DS using the `sm64config.txt` configuration file.

[This](https://codepen.io/benoitcaron/full/abNZrbP) online editor from [BenoitCaron](https://github.com/BenoitCaron) is usable, but it does not support remapping of all buttons.

## Caveats and Limitations

- All of the N64 and 3DS buttons are available to remap except for the N64 Analog Stick and 3DS Circle Pad, as their mapping is hard-coded.
- Only the Player 1 controller can be used.
- On older versions of the 3DS port, and also currently on the PC port, the N64 D-Pad was not mappable, as the vanilla game only uses it for debug controls. This port allows mapping the N64 D-Pad, so ensure that your version is up-to-date if you wish to use it.

## How to Find `sm64config.txt`

Run the game once and search your SD card for `sm64config.txt`.
- When running from a 3DSX, it should appear next to the 3DSX file.
- When running from an installed CIA file, it should appear in your SD card's root.
- When launching remotely via 3DSLink, it should appear in your SD card's root.

## How to Edit `sm64config.txt`

Inside of `sm64config.txt,` you will find various `key_` variables, followed by a number, and separated by a space. The `key_` variables represent the N64 controller's inputs, and the number represents the 3DS's inputs.

Every available 3DS input is represented by a single bit in a binary number. For example, A is the first bit, and B is the second bit. The full list can be found [below](#3ds-button-definitions). However, these numbers are stored in `sm64config.txt` as decimal numbers, which is why they may look nonsensical.

To map a button, simply copy the button code from the [definitions](#3ds-button-definitions) below into your `sm64config.txt,` replacing the existing number for that `key_` input.

To unmap a button, replace its button code with a `0.`

## Mapping Multiple 3DS Buttons to One N64 Button

Mapping multiple 3DS buttons to one N64 button requires mathematically adding each button code.

For example, to map the N64 Z Button to both the 3DS L Button and the 3DS R button, do as follows:
```
3DS L Button: 512
3DS R Button: 256

512 + 256 = 768
```

Then, write `key_z 768` to `sm64config.txt,` replacing the existing value if required.

## 3DS Button Definitions
The following 3DS buttons are supported by SM643DS.
```
    Code        3DS Button
─────────────┬────────────────
         1   │    A Button
         2   │    B Button
         4   │    Select Button
         8   │    Start Button
        16   │    D-Pad Right
        32   │    D-Pad Left
        64   │    D-Pad Up
       128   │    D-Pad Down
       256   │    R Button
       512   │    L Button
      1024   │    X Button
      2048   │    Y Button
     16384   │    ZL Button (New 3DS only)
     32768   │    ZR Button (New 3DS only)
  16777216   │    C-Stick Right (New 3DS only)
  33554432   │    C-Stick Left (New 3DS only)
  67108864   │    C-Stick Up (New 3DS only)
 134217728   │    C-Stick Down (New 3DS only)
```

## Notes

- The 3DS Circle Pad can actually be bound to N64 buttons, but it cannot be unbound from the N64 Analog Stick from within `sm64config.txt,` so its codes were omitted here.
- The Touch Screen's virtual buttons cannot currently be edited.
- When running on the Old 3DS, mapping buttons only present on the New 3DS is harmless.
- If duplicate config entries exist in `sm64config.txt,` the last entry will be the only one used.
