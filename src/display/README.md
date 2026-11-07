# OLED controller selection

The firmware supports 128x64 I2C SSD1306 and SH1106 modules at address
`0x3c`. Select the controller when configuring the firmware:

```sh
cmake -S . -B build/ssd1306 -DDISPLAY_CONTROLLER=SSD1306
cmake -S . -B build/sh1106 -DDISPLAY_CONTROLLER=SH1106
cmake -S . -B build/auto -DDISPLAY_CONTROLLER=AUTO
```

Add the usual board and Bluetooth options to these commands. SSD1306 is
the default for all boards, preserving the existing initialization,
horizontal transfers and 1 MHz bus rate. SH1106 uses 400 kHz, its DC/DC
enable command, and eight page transfers with a two-column RAM offset.
Both share the framebuffer, fonts, status display and runtime menus.
Auto uses 400 kHz and probes the SH1106 status register before DMA starts.
It requires the display-off bit to follow an off/on/off command sequence,
with the busy and reserved-zero bits clear, then restores display-off.
Each I2C operation has a 25 ms timeout. Failed writes, missing read support,
or unexpected replies select SSD1306. No detected choice is saved to flash.

This is a heuristic, not a universal chip ID: clones may behave differently.
Use an explicit controller setting if Auto selects the wrong driver. The
SSD1306 default is unchanged. An address acknowledgement alone cannot
identify either controller.

The status probe follows the I2C read protocol and status bits in the
[SH1106 datasheet](https://ssd1306.readthedocs.io/en/latest/_downloads/SH1106.pdf)
(pages 11 and 28). Auto needs testing on real modules of both types.

The SH1106 mode targets the common 128x64 modules whose visible columns
start at RAM column 2. Check all four edges, menu/status transitions and
blanking on the actual module before relying on it. Compilation and wire
format checks do not establish physical display compatibility.

Reference: [Adafruit's SH1106 driver](https://github.com/adafruit/Adafruit_SH110x/blob/master/Adafruit_SH1106G.cpp).

# ugui

this path contains the excellent µgui library from achim döbler.

this code can be obtained from [github.com/achimdoebler/UGUI](https://github.com/achimdoebler/UGUI). the license follows.

## license

```
/* -------------------------------------------------------------------------------- */
/* -- µGUI - Generic GUI module (C)Achim Döbler, 2015                            -- */
/* -------------------------------------------------------------------------------- */
// µGUI is a generic GUI module for embedded systems.
// This is a free software that is open for education, research and commercial
// developments under license policy of following terms.
//
//  Copyright (C) 2015, Achim Döbler, all rights reserved.
//  URL: http://www.embeddedlightning.com/
//
// * The µGUI module is a free software and there is NO WARRANTY.
// * No restriction on use. You can use, modify and redistribute it for
//   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
// * Redistributions of source code must retain the above copyright notice.
//
/* -------------------------------------------------------------------------------- */
/* -- MY SPECIAL THANKS GO TO                                                    -- */
/* -------------------------------------------------------------------------------- */
// Andrey Filimonov (-->https://github.com/Sermus)
// for giving valuable suggestions, reporting bugs and adding several new features.
// Andrey also put a lot of work in the implementaion of anti-aliased font support.
//
// Mikhail Podkur (-->https://github.com/MikhailPodkur)
// for adding cyrillic 8x12 font, checkbox feature and RGB565 support.
//
// Gustavo Denardin
// for giving valuable suggestions regarding real-time os support.
//
// Samuel Kleiser
// for reporting bugs and giving examples how to improve µGUI.
/* -------------------------------------------------------------------------------- */
```
