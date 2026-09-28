# Joystick mode

The `features` firmware supports a one-button digital joystick. On REV5 boards,
port 1 defaults to mouse and port 2 to joystick, working simultaneously. Use a
`BOARD_HIDPICO_REV5` build for this arrangement. Keyboard input continues normally,
including access to the settings menu.

## Setup

1. Connect the board's first controller cable to Amiga port 1 and its second cable
   to Amiga port 2. The actual Amiga sockets determine which port software sees.
2. Connect a supported USB HID gamepad or joystick. The OLED's USB `j` count
   increases when a supported controller descriptor is found. For Bluetooth LE,
   use the [Stadia setup](#stadia-bluetooth) below.
3. The REV5 defaults are **Controller port 1: Mouse** and
   **Controller port 2: Joystick**. To check or change them, hold F12 for one
   second, release it, and use the settings menu. Select **Save and exit** after edits.
4. Centre the stick/D-pad and release all buttons before testing directions and fire.

The stick/D-pad provides up, down, left, right and diagonals. Generic HID Button 1
provides fire; PS3 USB mode uses the four face buttons instead. The saved
mode survives power loss. Port 2 can be turned Off independently. If port 1 is
configured as Joystick, select Mouse there to restore mouse input.

REV2 and REV4 builds support only controller port 1, where Mouse and Joystick
remain alternative modes. REV4's second port has a hardware wiring error; see
the [errata](errata.md#board-revision-4). Do not use a REV5 build on an unmodified
REV4 board. The REV5 port-2 mapping is Up GPIO27, Down GPIO26, Left GPIO22,
Right GPIO21 and Fire GPIO20; the other two button lines remain released.

All connected USB and LE gamepads currently feed one combined joystick state. If both REV5
ports are configured as joystick, both output that state; independent player
assignment and mouse on port 2 are not implemented.

## Supported input

- USB HID Game Pad or Joystick application collections.
- Four- or eight-way hat switches, or separate HID D-pad direction usages.
- Absolute X/Y fields mapped to digital directions. These include digital sticks
  such as the Competition Pro, which reports its switches as X/Y values.
- HID Button 1 for fire.
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
directions cancel. Deadzone and button mapping are not yet configurable.

Bluetooth Classic gamepads (including PS4), proprietary/XInput reports,
keyboard-as-joystick and CD32 buttons are not implemented yet. Other LE controllers
with standard HID gamepad reports may work, but have not been validated.
The parser supports up to four input report IDs, sixteen mapped fields and
64-byte reports. Unsupported descriptors are ignored.

Opening the menu releases all joystick controls. After closing it or switching
modes, centre the gamepad and release buttons before input resumes. Unplugging a
controller releases its controls; other connected controllers retain their
state. Opposing directions cancel when multiple controllers are active.

## Stadia Bluetooth

The Stadia controller must be converted to Bluetooth mode before pairing.
Use the Pico W USB + Bluetooth build for your board revision.

1. Keep **Controller port 1: Mouse** and **Controller port 2: Joystick** on REV5.
2. Turn on the controller and put it in pairing mode: hold **Stadia + Y** for
   two seconds until its light pulses orange. See
   [Google's pairing instructions](https://support.google.com/stadia/answer/13067284?hl=en).
3. The Pico scans automatically. OLED status progresses through `le:scan`,
   `le:conn`, `le:pair` and service discovery. `le:pad` is gamepad report discovery;
   **`bt c0 le:ready j1`** means the LE gamepad report map has been accepted.
   The `c` number counts Classic connections, so `c0` is normal for Stadia.
4. Use the **D-pad or left stick** for directions and **A** for fire. Centre the
   stick and release all controls after leaving the settings menu.
5. Test directions, diagonals and fire in Amiga Test Kit, then turn off and
   reconnect the controller. Disconnecting must release held directions and fire.

Only one LE device can be connected at a time in this implementation. USB and
Classic Bluetooth keyboards/mice can remain connected. Rumble, right-stick
mapping, extra Amiga buttons and separate player assignments are not implemented.

The LE path reads HID Report Maps and Report References and subscribes to input
notifications using BTstack's HIDS host. Existing LE boot keyboards/mice retain
their boot-report path. Standard gamepad reports use the same bounded decoder as
USB; gamepad axes are never passed to the mouse decoder on this LE path.

## Diagnosing missing input

Hold F12, select **Display: HID diagnostics**, then **Save and exit**.
With a supported controller connected, the two diagnostic rows show, for example:

```text
j002a l09/09 d+ r+
in01 out01 p2
```

- `j`: received report count in hexadecimal, wrapping after `ffff`. It resets on
  reconnect. If it stays at zero while moving the stick, no input packets arrived.
- `l`: actual/descriptor report length in decimal, including any report ID byte.
  An unknown report ID shows an expected length of zero.
- `d+` / `d-`: the latest packet decoded successfully / did not decode. Before
  the first packet, `d-` and `in--` mean no input has been decoded yet.
- `r+` / `r-`: the last request for the next USB report succeeded / failed.
  Success means the transfer was queued; it does not prove packets are arriving.
- `in`: decoded controls; `--` means the latest packet could not be decoded.
- `out`: combined controls submitted to the output core after routing and menu
  capture. This is a software state, not a measurement of the DB9 signals.
- `p`: enabled joystick ports: `0` none, `1` first, `2` second, `3` both.
- `wait`: input is captured by the menu or waiting for a neutral report after
  leaving it. Centre the stick and release all buttons; move it and centre it
  again if it sends reports only when controls change.

Controls are hexadecimal bit masks: up `01`, down `02`, left `04`, right `08`,
fire `10`; combinations add together. Neutral is `00`.
The input diagnostics follow the first mounted controller slot; output includes
all controllers. Reconnect resets that controller's counters. Updates are limited
to ten per second and Bluetooth pairing messages take priority on the fourth row.

Before the first report, the fourth row instead shows the USB identity and
initialization status, for example `usb 054c:0268 s+`. The status is `s-` for a
generic device needing no startup command, `sp` pending submission, `s?` awaiting
completion, `s+` completed, or `s!` failed. Completion alone does not prove input
has started; check that the `j` counter advances when moving the stick.

With a Bluetooth LE gamepad and no USB joystick connected, the rows show:

```text
b002a l11/11 d+
in01 out01 p2
```

`b` counts Bluetooth reports; the other fields have the same meanings as above.
Lengths include a Report ID when the HID descriptor uses one. Bluetooth input
arrives through notifications, so there is no USB `r` field. USB joystick
diagnostics take priority if both are connected; unplug the USB joystick when
checking Bluetooth reports. The first line's USB device counts exclude Bluetooth.

## Controller compatibility and references

Competition Pro controllers can expose different USB report formats. The
Competition Pro Extra (`0079:181c`, identifying as
`SPEEDLINK COMPETITION PRO Game Controller for Android`) reports its digital
stick as X/Y values of 0, 128 and 255. PS3-compatible mode uses a different
report layout and needs the startup command described below. If diagnostics
show `j0000 l00/49 d- r+`, check the initialization status: a queued receive
request alone does not mean the controller has started sending input.

Stadia report-format references are the published
[Bluetooth descriptor](https://github.com/libsdl-org/SDL/issues/7224#issuecomment-1445457085)
and [SDL's Stadia driver](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_stadia.c).

The PS3 startup transfer is feature report `f4`, payload `42 0c 00 00`, as used
by the [USB Host Shield PS3 driver](https://github.com/felis/USB_Host_Shield_2.0/blob/master/PS3USB.cpp#L444).
A [Competition Pro adapter implementation](https://www.hackster.io/DocSnyderde/connect-usb-joystick-to-commodore-c64-2fb5ba)
uses that driver and maps the physical fire buttons to the PS3 face buttons.

The parser follows [USB HID 1.11](https://www.usb.org/sites/default/files/hid1_11.pdf)
and the [HID Usage Tables](https://www.usb.org/sites/default/files/hut1_2.pdf).
Output uses the digital joystick pin assignments in Commodore's
[Amiga Hardware Reference Manual, Appendix E](https://www.theflatnet.de/pub/cbm/amiga/AmigaDevDocs/hard_e.html).
