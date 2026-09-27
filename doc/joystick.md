# USB joystick mode

The `features` firmware can drive the existing Amiga controller connector as a
one-button digital joystick. Mouse remains the default mode. Keyboard input
continues to work in either mode, including access to the settings menu.

## Setup

1. Connect a supported USB HID gamepad or joystick. The OLED's `j` count increases
   when a supported controller descriptor is found.
2. Hold F12 for one second, then release it to open the settings menu.
3. Set **Controller port** to **USB joystick**, then select **Save and exit**.
4. Centre the stick/D-pad and release all buttons before testing directions and fire.

The stick/D-pad provides up, down, left, right and diagonals. HID Button 1 provides
fire; the physical label for that button depends on the controller. The saved
mode survives power loss. Choose **Mouse** and save to return to mouse input.

The cable's Amiga port determines which joystick port software sees. Games often
expect port 2, while the mouse normally uses port 1; use software that can select
the connected port, or move the controller cable as appropriate. This mode uses
the same connector and GPIOs as mouse emulation, so it provides one controller
port at a time.

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
actual controller GPIO output. REV4 builds pass for Pico W USB + Bluetooth
(Release and Debug) and USB-only Pico (Release). Gamepad behavior on an actual
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
