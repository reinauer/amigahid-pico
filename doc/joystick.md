# Joystick mode

The firmware supports a three-button digital joystick and CD32 pad emulation. On REV5 boards,
port 1 defaults to mouse and port 2 to joystick, working simultaneously. Use a
`BOARD_HIDPICO_REV5` build for this arrangement. Keyboard input continues normally,
including access to the settings menu.

## Setup

1. Connect the board's first controller cable to Amiga port 1 and its second cable
   to Amiga port 2. The actual Amiga sockets determine which port software sees.
2. Connect a supported USB HID gamepad or joystick. The OLED's USB `j` count
   increases when a supported controller descriptor is found. For Bluetooth,
   use the [Stadia](#stadia-bluetooth) or [PS4](#ps4-bluetooth)
   instructions below.
3. The REV5 defaults are **Controller port 1: Mouse** and
   **Controller port 2: Joystick**. To check or change them, hold F12 for one
   second, release it, and use the settings menu. Select **Save and exit** after edits.
4. Centre the stick/D-pad and release all buttons before testing directions and fire.

The stick/D-pad provides up, down, left, right and diagonals. Generic HID Buttons
1, 2 and 3 supply the three fire buttons; Button 4 is used for the third fire when
Button 3 is absent from the descriptor (as on Stadia). PS3 USB mode retains its
four-face-buttons-as-fire-1 mapping for the Competition Pro. The saved
mode survives power loss. Port 2 can be turned Off independently. Select Mouse
on port 1 to enable mouse input.

REV2 and REV4 builds support only controller port 1, where Mouse and Joystick
remain alternative modes. REV4's second port has a hardware wiring error; see
the [errata](errata.md#board-revision-4). Do not use a REV5 build on an unmodified
REV4 board. The REV5 port-2 mapping is Up GPIO27, Down GPIO26, Left GPIO22,
Right GPIO21 and buttons 1/2/3 on GPIO20/19/18 respectively.

The Amiga can read buttons 1/2/3 on DE-9 pins 6/9/5. Amiga Test Kit displays
these separately in Joystick mode; its `2` and `3` labels are always drawn and
do not indicate that a particular controller was detected. Games must explicitly
read the extra buttons to use them. See the
[hardware pinout](https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_e.html)
and [Amiga Test Kit's button reader](https://github.com/keirf/amiga-stuff/blob/master/testkit/joymouse.c).

All connected USB and Bluetooth gamepads currently feed one combined joystick state. If both REV5
ports are configured as joystick, both output that state; independent player
assignment and mouse on port 2 are not implemented.

## Supported input

- USB HID Game Pad or Joystick application collections.
- Four- or eight-way hat switches, or separate HID D-pad direction usages.
- Absolute X/Y fields mapped to digital directions. These include digital sticks
  such as the Competition Pro, which reports its switches as X/Y values.
- Three fire buttons from HID Button usages 1, 2 and 3 (or 4 if 3 is absent).
- Report IDs, including directions and fire in separate reports.
- PS3 USB controllers identifying as `054c:0268` with input report ID 1 and a
  49-byte report. An asynchronous feature-report command enables input; the
  D-pad and descriptor-defined X/Y axes supply directions, and the four face
  buttons supply fire. This includes the PS3 mode used by some Competition Pro
  controllers. The device identity and report layout must both match before
sending the startup command. Other HID devices keep generic handling.

X/Y values in the central half of the descriptor's logical range are neutral;
values in the outer quarters select a direction. Low X means left, low Y means
up. Relative axes are ignored. Hats and X/Y can both supply directions; opposing
directions cancel. Deadzone and button mapping are configurable in the OLED menu; see below.

Bluetooth LE and Classic gamepads with standard HID reports use the generic
decoder. DualShock 4 Bluetooth has a separate decoder for its simple and extended
reports, with Cross/Circle/Square as fire buttons and touchpad mouse input. Proprietary/XInput reports,
and keyboard-as-joystick are not implemented yet. Controllers other
than the tested PS4, Stadia and Competition Pro still need hardware validation.
The generic parser supports up to four input report IDs, thirty-two mapped fields
and 64-byte reports. Unsupported controller layouts are not treated as mice.

Opening the menu releases all joystick controls. After closing it or switching
modes, centre the gamepad and release buttons before input resumes. Unplugging a
controller releases its controls; other connected controllers retain their
state. Opposing directions cancel when multiple controllers are active.

## Stadia Bluetooth

Stadia controllers must already be converted to Bluetooth mode.
Use the Pico W USB + Bluetooth build for your board revision.

1. Keep **Controller port 1: Mouse** and **Controller port 2: Joystick** on REV5.
2. Turn on the controller and put it in pairing mode: hold **Stadia + Y** for
   two seconds until its light pulses orange. See
   [Google's pairing instructions](https://support.google.com/stadia/answer/13067284?hl=en).
3. The Pico scans automatically. OLED status progresses through `le:scan`,
   `le:conn`, `le:pair` and service discovery. `le:pad` is gamepad report discovery;
   **`bt c0 le:ready j1`** means the LE gamepad report map has been accepted.
   The `c` number counts Classic connections, so `c0` is normal for Stadia.
4. Use the **D-pad or left stick** for directions and **A/B/X** for joystick
   buttons **1/2/3**. Centre the stick and release all controls after leaving
   the settings menu. Y remains unmapped.
5. Test directions, diagonals and fire in Amiga Test Kit, then turn off and
   reconnect the controller. Disconnecting must release held directions and fire.

Only one LE device can be connected at a time in this implementation. USB and
Classic Bluetooth keyboards/mice can remain connected. Rumble, right-stick
mapping and separate player assignments are not implemented.
Stadia pairing and A/B/X buttons have been tested on REV5. Reconnection and
extended use remain unverified.

## PS4 Bluetooth

Use the Pico W USB + Bluetooth REV5 build. Keep **Controller port 1: Mouse**
and **Controller port 2: Joystick**.

1. Disconnect USB from the DualShock 4. If it is on, hold PS until it turns off.
2. Hold **SHARE + PS** until the light bar flashes. See
   [Sony's pairing instructions](https://www.playstation.com/en-us/support/hardware/ps4-pair-dualshock-4-wireless-with-pc-or-mac/).
3. The Pico searches for Classic Bluetooth gamepads automatically in short rounds,
   with five-second pauses. `c0:scan` means Classic discovery; `c0:conn` means a
   connection attempt. A successful PS4 connection normally shows
   **`bt c1 le:scan j1`**. `le:scan` is the independent LE scanner, so it is normal
   for it to remain visible while using a PS4 controller.
4. Use the D-pad or left stick for directions, with **Cross/Circle/Square** as
   joystick buttons **1/2/3** on port 2. Triangle remains unmapped.
5. Move one finger on the touchpad to move the mouse on port 1. Mouse buttons:
   **Touchpad press = left**, **L1 = middle**, **R1 = right**.
   **L2/R2** are reserved for CD32 Rewind/Forward by default.
   Simultaneous mouse buttons are supported. Lifting
   and replacing a finger starts a new movement without jumping the pointer.
6. After pairing, try switching the controller off and reconnecting with PS.
   Held directions, fire and mouse buttons must release on disconnect.

The firmware identifies the standard DualShock 4 Bluetooth report descriptor
before requesting feature report 2 to enable extended input. It handles
10-byte report 1 and 78-byte report 17, including the latter's CRC. With HID
diagnostics enabled and no USB joystick attached, `l78/78 d+` confirms that
extended packets are arriving and decoding; `l10/10 d+` means basic joystick
input without touch movement. The first USB device-count line excludes Bluetooth.

Discovery stops once a Classic gamepad is accepted and resumes after it disconnects.
One Classic gamepad is discovered automatically; the two Classic HID connection
slots can also serve a keyboard or mouse. A Stadia/other LE device can coexist.
All gamepads still share one joystick output state. Touch gestures,
rumble, light-bar control and motion sensors are not supported. Controller
clones with different descriptors are not identified as DualShock 4.

DualShock 4 joystick buttons and touchpad mouse input have been tested on REV5.
Reconnection and extended use remain unverified. Mouse-button holds are tracked
independently across USB and Bluetooth devices: releasing one device cannot
release a button held on another.

## Diagnosing missing input

Hold F12, select **Display: HID diagnostics**, then **Save and exit**.
With a supported controller connected, the two diagnostic rows show, for example:

```text
j002a l09/09 d+ r+
in001 out001 p2
```

- `j`: received report count in hexadecimal, wrapping after `ffff`. It resets on
  reconnect. If it stays at zero while moving the stick, no input packets arrived.
- `l`: actual/descriptor report length in decimal, including any report ID byte.
  An unknown report ID shows an expected length of zero.
- `d+` / `d-`: the latest packet decoded successfully / did not decode. Before
  the first packet, `d-` and `in---` mean no input has been decoded yet.
- `r+` / `r-`: the last request for the next USB report succeeded / failed.
  Success means the transfer was queued; it does not prove packets are arriving.
- `in`: decoded controls; `---` means the latest packet could not be decoded.
- `out`: combined controls submitted to the output core after routing and menu
  capture. This is a software state, not a measurement of the DB9 signals.
- `p`: enabled joystick ports: `0` none, `1` first, `2` second, `3` both.
- `wait`: input is captured by the menu or waiting for a neutral report after
  leaving it. Centre the stick and release all buttons; move it and centre it
  again if it sends reports only when controls change.

Controls are hexadecimal bit masks: up `001`, down `002`, left `004`, right `008`,
fire 1 `010`, fire 2 `020`, fire 3 `040`; combinations add together. Neutral is `000`.
The input diagnostics follow the first mounted controller slot; output includes
all controllers. Reconnect resets that controller's counters. Updates are limited
to ten per second and Bluetooth pairing messages take priority on the fourth row.

Before the first report, the fourth row instead shows the USB identity and
initialization status, for example `usb 054c:0268 s+`. The status is `s-` for a
generic device needing no startup command, `sp` pending submission, `s?` awaiting
completion, `s+` completed, or `s!` failed. Completion alone does not prove input
has started; check that the `j` counter advances when moving the stick.

With a Bluetooth gamepad and no USB joystick connected, the rows show:

```text
b002a l11/11 d+
in001 out001 p2
```

`b` counts Bluetooth reports; the other fields have the same meanings as above.
Lengths include a Report ID when the HID descriptor uses one. Bluetooth input
arrives through LE notifications or Classic HID packets, so there is no USB `r` field. USB joystick
diagnostics take priority if both are connected; unplug the USB joystick when
checking Bluetooth reports. A Classic gamepad takes precedence over an LE gamepad
in diagnostics. The first line's USB device counts exclude Bluetooth.

## Controller compatibility

Competition Pro controllers can expose different USB report formats. The
Competition Pro Extra (`0079:181c`) reports its digital stick as X/Y values of
0, 128 and 255 in generic HID mode. Its PS3-compatible mode uses a different
report layout and requires the firmware's startup command. If diagnostics show
`j0000 l00/49 d- r+`, check the initialization status: a queued receive request
does not mean the controller has started sending input.

Competition Pro joystick input, Stadia and DualShock 4 have been tested on REV5.
Other controllers may work when they use a supported HID layout; compatibility
is not guaranteed by the device being described as a USB or Bluetooth gamepad.

## Gamepad settings

The OLED menu selects D-pad and stick together (default), D-pad only, or
stick only. Stick deadzone selects the central 25%, 50% (default), or 75% of
travel; directions activate outside that range. A wider deadzone requires more
stick movement. These settings apply to USB and Bluetooth controllers.

Each of seven output buttons has an Auto mapping, a HID button number 1–16, or
Disabled. The first three are the ordinary joystick buttons; the other four
are retained for CD32 output. Mappings are global across connected controllers.
Auto uses the PS4 Cross/Circle/Square and Stadia A/B/X mapping.
PS4's additional defaults are Triangle, L2, R2, Options; Stadia's are Y, L1,
R1, Menu. The Competition Pro retains all four face buttons as fire 1.

For custom mappings, PS4 HID numbers are Square=1, Cross=2, Circle=3,
Triangle=4, L1=5, R1=6, L2=7, R2=8, Share=9, Options=10.
Stadia uses A=1, B=2, X=4, Y=5, L1=7, R1=8, Menu=12.
**PS4 mouse controls** selects Touchpad + shoulders (the default),
Touchpad only, or Off, so shoulders can be dedicated to gamepad use.

Save and exit applies these choices. Release held controls before resuming.
Diagnostics use three hexadecimal digits; extra output bits are fourth button
`080`, rewind `100`, forward `200`, and play `400`.
Custom gamepad settings still require hardware validation.

## CD32 pad mode

Keep Controller port 1 as Mouse and select **Controller port 2: CD32 pad**, then
Save and exit. In Amiga Test Kit select **CD32 Pad** for Amiga port 2 (F2 cycles
its type). Check **Pad Detected**, directions, all seven buttons and release states.
Port 1 can also select CD32; REV2/REV4 support CD32 on port 1 only. Both enabled
controller ports currently receive the same combined gamepad state.

| CD32 button | PS4 default | Stadia default |
| --- | --- | --- |
| Red | Cross | A |
| Blue | Circle | B |
| Green | Square | X |
| Yellow | Triangle | Y |
| Rewind | L2 | L1 |
| Forward | R2 | R1 |
| Play/Pause | Options | Menu |

The seven Gamepad mapping settings override these defaults. PS4 L1/R1 control
middle/right mouse buttons, while L2/R2 control CD32 Rewind/Forward. To disable
shoulder mouse clicks, select **PS4 mouse controls: Touchpad only**; touch movement and
pad-click remain available on the mouse port. Explicit custom gamepad button
mappings still override these defaults.

CD32 uses Amiga pin 5 as select, pin 6 as clock and pin 9 as serial data. PIO
handles both data and fire/clock ownership. Data is actively driven during
serial reads; normal buttons and fire/clock use open-drain output. The select
line is never driven in CD32 mode. Each read snapshots all buttons and supplies
the identification bits after them. With select high, Red and Blue behave as
ordinary joystick buttons. CD32 mode does not provide the third ordinary button
on pin 5, because that pin belongs to the Amiga's select signal.

PIO1 supplies two state machines per port and shares 30 instructions between
ports. PIO0 remains available to CYW43. Clock handling continues through CPU
interrupts and flash lockout. Direction pins remain serviced by the output core.
Reference: [Amiga Test Kit's read_gamepad implementation](https://github.com/keirf/amiga-stuff/blob/master/testkit/joymouse.c).

CD32 serial output is implemented but remains unverified on physical hardware
with the current data-line drive behavior. Check pad detection and each button
in Amiga Test Kit before relying on CD32 mode in games.
