# AmigaHID Preferences

`AmigaHIDPrefs` is a native Workbench utility for reading and editing settings
on the Pico through its existing second controller cable. It requires AmigaOS
2.04 or newer, a standard Amiga controller port, and an AmigaHID REV5 board
running the matching firmware. The executable supports the 68000
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

## Compatibility

The utility uses the existing port-2 cable; no serial adapter is required.
It claims the Amiga's second gameport and POT resources while open, and releases
them on exit. Other software must leave that controller port available.

The utility and firmware must support the same settings format. An incompatible
reply is rejected; install matching versions if the utility reports a format
mismatch. REV2/REV4 firmware does not provide this link.

The port-2 link and Workbench interface are experimental and still need testing
on an Amiga. Before relying on saved changes, use Read to verify them and check
that they persist after power cycling. Closing the utility or holding F12 must
restore normal controller operation.
