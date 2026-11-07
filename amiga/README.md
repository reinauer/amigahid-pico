# AmigaHIDPrefs

Native AmigaOS 2.04+ preferences utility for REV5 AmigaHID-Pico boards.
Build with `make` and an `m68k-amigaos-gcc` toolchain; output is placed under
`../build/amiga-prefs/`.

Enable **Amiga preferences** in the Pico's OLED menu before clicking **Read**.
Communication uses the existing controller-port-2 cable. Mouse on port 1 and
keyboard input remain available. The matching Pico firmware is required.

See [setup, actions, build and protocol notes](../doc/amiga-preferences.md).
