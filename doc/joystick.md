# USB joystick mode

The `features` firmware supports a one-button digital joystick. On REV5 boards,
port 1 defaults to mouse and port 2 to joystick, working simultaneously. Use a
`BOARD_HIDPICO_REV5` build for this arrangement. Keyboard input continues normally,
including access to the settings menu.

## Setup

1. Connect the board's first controller cable to Amiga port 1 and its second cable
   to Amiga port 2. The actual Amiga sockets determine which port software sees.
2. Connect a supported USB HID gamepad or joystick. The OLED's `j` count increases
   when a supported controller descriptor is found.
3. The REV5 defaults are **Controller port 1: Mouse** and
   **Controller port 2: USB joystick**. To check or change them, hold F12 for one
   second, release it, and use the settings menu. Select **Save and exit** after edits.
4. Centre the stick/D-pad and release all buttons before testing directions and fire.

The stick/D-pad provides up, down, left, right and diagonals. HID Button 1 provides
fire; the physical label for that button depends on the controller. The saved
mode survives power loss. Port 2 can be turned Off independently. If an earlier
trial saved port 1 as USB joystick, select Mouse there to restore mouse input.

REV2 and REV4 builds support only controller port 1, where Mouse and USB joystick
remain alternative modes. REV4's second port has a hardware wiring error; see
the [errata](errata.md#board-revision-4). Do not use a REV5 build on an unmodified
REV4 board. The REV5 port-2 mapping is Up GPIO27, Down GPIO26, Left GPIO22,
Right GPIO21 and Fire GPIO20; the other two button lines remain released.

All connected gamepads currently feed one combined joystick state. If both REV5
ports are configured as joystick, both output that state; independent player
assignment and mouse on port 2 are not implemented.

## Supported input

- USB HID Game Pad or Joystick application collections.
- Four- or eight-way hat switches, or separate HID D-pad direction usages.
- Absolute X/Y fields mapped to digital directions. These include digital sticks
  such as the Competition Pro, which reports its switches as X/Y values.
- HID Button 1 for fire.
- Report IDs, including directions and fire in separate reports.

X/Y values in the central half of the descriptor's logical range are neutral;
values in the outer quarters select a direction. Low X means left, low Y means
up. Relative axes are ignored. Hats and X/Y can both supply directions; opposing
directions cancel. Deadzone and button mapping are not yet configurable.

Bluetooth gamepads, proprietary/XInput reports, keyboard-as-joystick and CD32
buttons are not implemented yet.
The parser supports up to four input report IDs, sixteen mapped fields and
64-byte reports. Unsupported descriptors are ignored.

Opening the menu releases all joystick controls. After closing it or switching
modes, centre the gamepad and release buttons before input resumes. Unplugging a
controller releases its controls; other connected controllers retain their
state. Opposing directions cancel when multiple controllers are active.

## Validation

Temporary native checks cover descriptor parsing, malformed/truncated reports,
report IDs, settings migration, menu capture, mode changes, disconnects and the
actual controller GPIO output. REV5 builds pass for Pico W USB + Bluetooth
(Release and Debug) and USB-only Pico (Release), with a REV4 Release regression
build. Native checks cover concurrent mouse/joystick output, the REV5 pin map,
port 2 disable, capture, source disconnects and settings migration. Gamepad behavior on an actual
Amiga still needs testing, including each direction, diagonals, fire, unplugging,
menu entry and returning to mouse mode. No new tests are checked in.

The connected Competition Pro Extra (`0079:181c`, identifying as
`SPEEDLINK COMPETITION PRO Game Controller for Android`) was inspected on Linux.
Its digital stick reports X/Y values of 0, 128 and 255; its advertised hat remained
centred. Native checks using its actual descriptor and captured axis values pass
for neutral, all four directions and all four diagonals. All four physical buttons
produced input events; the current firmware maps only HID Button 1 to fire.
This verifies input decoding, not the complete Pico-to-Amiga path.

The parser follows [USB HID 1.11](https://www.usb.org/sites/default/files/hid1_11.pdf)
and the [HID Usage Tables](https://www.usb.org/sites/default/files/hut1_2.pdf).
Output uses the digital joystick pin assignments in Commodore's
[Amiga Hardware Reference Manual, Appendix E](https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_e.html).
