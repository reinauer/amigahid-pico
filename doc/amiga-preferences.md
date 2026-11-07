# AmigaHID Preferences

`AmigaHIDPrefs` is a native Workbench utility for reading and editing settings
on the Pico through its existing second controller cable. This first version
requires AmigaOS 2.04 or newer, a standard Amiga controller port, and an AmigaHID
REV5 board running the matching firmware. The executable supports the 68000
and later CPUs, including the A3000's 68030. No `lowlevel.library` is required.

## Using the utility

1. Connect AmigaHID controller port 2 to **Amiga controller port 2**. Keep port 1
   connected as the mouse. Close software that is using the joystick port.
2. Hold F12 to open the Pico menu. Save or cancel any pending edits, then select
   **Amiga preferences** and press Enter. The OLED shows **Port 2 link enabled**.
3. Copy `AmigaHIDPrefs` and `AmigaHIDPrefs.info` together to the Amiga. Launch
   it from Workbench or the Shell and click **Read**. It shows
   the firmware version and loads the current settings.
4. Choose a **Group**, **Setting**, and **Value**. Changes stay in the editor
   until you click **Use** or **Save**.
5. Close the utility when finished. Port 2 returns to its configured joystick,
   CD32, or Off mode. Release the controller buttons and centre the stick before
   using it again.

Mouse and keyboard input remain available during the session. Joystick/CD32
output on port 2 is suspended. Enabling the link is temporary and is never saved
in flash. To cancel it locally, hold F12 even if another menu key or boot-only
entry is configured. Five minutes without a completed transfer also ends it.
If it expires while editing, re-enable it in the Pico menu and retry Use/Save;
clicking Read would replace the edits with the current Pico settings.

| Action | Result |
| --- | --- |
| Read | Reload the Pico's current active settings, replacing editor changes. |
| Defaults | Load factory defaults into the editor; apply with Use or Save. |
| Use | Apply the edited settings in RAM until the next power-cycle. |
| Save | Apply and save the edited settings to Pico flash. |
| Cancel / close | Discard unapplied edits and end the link. Earlier Use/Save actions remain in effect. |

There are 26 editable settings across General, Mouse, Keyboard, Gamepad, and
Bluetooth. They cover both controller modes, mouse timing/wheel, watchdog,
OLED/menu behavior, keyboard presets and special keys, all seven gamepad button
mappings, DS4 mouse controls, radio state, and pairing policy. The eight custom
key remaps are preserved; edit those in the OLED menu. Pairing and forgetting
Bluetooth devices also remain OLED actions. Defaults reset the complete settings
record, including custom remaps, when subsequently applied.

Keyboard shortcuts: **R** Read, **D** Defaults, **U** Use, **S** Save,
**Esc** Cancel, **G** next group, **N** next setting, **+/-** change value.
These also let you operate the utility after changing port 1 away from Mouse.
Bluetooth settings are disabled when connected to a USB-only firmware build.

If the utility reports that the port is in use, close the other joystick/pad
software and retry. A missing reply means the link may not be enabled, the cable
may be on the wrong port, or the firmware lacks preferences support. A failed
Save acknowledgement is not proof that the write failed: re-enable the link and
Read to check the current settings before retrying.

## Building

From the repository root, with a GCC AmigaOS cross-toolchain on PATH:

```sh
make -C amiga
```

Or specify its prefix:

```sh
make -C amiga AMIGA_PREFIX=/opt/amiga/bin/m68k-amigaos-
```

The output is `build/amiga-prefs/AmigaHIDPrefs`, a native Hunk executable,
plus its Workbench `.info` icon. The icon generator requires Python 3.
Firmware builds use the existing Pico CMake workflow. REV2/REV4 firmware keeps
its existing behavior and does not offer the port-2 preferences action.

## Link implementation

The link is explicitly enabled from the OLED menu. It uses DE-9 pin 5 as the
Amiga-driven clock, pin 9 as half-duplex data, and pin 6 as the Pico's
acknowledgement. Pico outputs only sink or release these lines in this mode.
The Amiga keeps its POT pins in current-limited output mode, driving data high
while receiving. This prevents the analogue counters from discharging the data
line during a transfer. The host waits at least 50 us for data to settle and
waits for each rising/falling edge to be acknowledged, allowing CPU/IRQ delays.

The first frame contains the `AH` signature, protocol version, command, and
sequence number. The same fixed 64-byte format is used for every request and
reply, with a length, status, zero padding and CRC16-CCITT-FALSE. All multi-byte
protocol values are explicitly little-endian. Settings wire version 1 contains
48 byte fields; it is independent of the on-flash record header and CRC.

Commands: Hello, Read, Defaults, Use, Save, End. The host makes at most two
attempts with the same sequence number. The firmware caches the last request
and response so a retried Save does not write flash twice. Partial transfers
reset after 500 ms without an edge; waiting for core0 permits two seconds.
The final End acknowledgement is held for 250 ms so the Amiga can release its
pins before normal controller output resumes. Complete inactivity ends the
session after five minutes. Canceling from the OLED menu releases the pins too.

Core1 owns the pins and disables CD32 PIO for port 2 before taking them over.
Core0 validates complete requests, handles flash writes using the existing
alternating records, releases held input, and applies the settings. A generation
number discards requests/replies left behind by a canceled transfer. Port 1
continues mouse operation; port 2 waits for neutral controls on restoration.

On the Amiga, the utility claims gameport.device unit 1 as a custom controller
and allocates only port 2's POT bits through potgo.resource. It refuses a busy
port. It restores its CIA direction bit, releases the POT allocation and frees
the gameport unit on exit. Communication does not disable Amiga interrupts.

References: [gameport ownership](https://wiki.amigaos.net/wiki/Gameport_Device),
Commodore's `potgo.resource/AllocPotBits` and `WritePotgo` autodocs, and
[Amiga Test Kit's controller reader](https://github.com/keirf/amiga-stuff/blob/master/testkit/joymouse.c).

## Validation and first hardware trial

Software checks cover the actual host transport against the Pico transport and
settings server, CRC corruption at every bit, every partial-frame length,
settings validation, defaults without applying, Use versus Save, persistence,
failed saves, duplicate requests, stale requests, cancellation and timeouts.
GPIO checks cover port-2 takeover and restoration from joystick/CD32 modes,
concurrent mouse operation, and release of held controls. Tests are temporary
and are not added to the repository.

Physical communication and the Workbench window still require hardware testing:

1. Read and confirm the displayed firmware version and watchdog setting.
2. Use a changed setting, Read it back, and power-cycle: the last saved setting
   should return.
3. Save a changed setting and power-cycle: the new setting should persist.
4. Load Defaults and Cancel: nothing should change on the Pico.
5. Close the window and verify mouse, keyboard and joystick/CD32 operation.
6. Re-enable the link and cancel it with F12; verify recovery without a reboot.

The previously delivered CD32/DS4 trial is separate. Its button fix is also in
this combined firmware, but neither physical link timing nor the CD32 electrical
change is proven by software simulation alone.
