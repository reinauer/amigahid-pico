# Runtime configuration

The firmware provides an OLED settings menu for USB and Bluetooth input.
It works with the OLED on REV2, REV4, REV5/REV6 and REV7 boards. Board selection and
GPIO wiring remain build-time options.

## Open and use the menu

Hold **F12 for one second**, then release it. USB and Bluetooth keyboards use
the same menu. Press Up/Down to select an item, Left/Right to change its value,
and Enter to change a value or activate an action. **Save and exit** writes
settings to flash and applies them. **Esc** or **Cancel changes** discards edits.

Factory defaults changes the values in the editor; select Save and exit to
apply and preserve them. Values are not previewed while editing.

### REV7 menu lever

The ALPS lever beside RECOVERY works without a USB or Bluetooth keyboard:

- Hold the inward push for one second, then release, to open the menu.
- Rock left/right to browse items. Holding a direction repeats after a pause.
- Tap push on a setting to edit it; rock left/right to change its value,
  then tap push again to return to browsing. The bottom line shows the mode.
- Tap push on an action to activate it. **Save and exit** saves and applies
  edits; **Cancel changes** discards them. Forget BT devices needs two taps.
- Hold push for one second inside the menu to cancel and exit. While learning
  a custom source key, tap or hold push cancels learning first; a keyboard is
  still needed to supply the actual source key.

The lever can open the menu even with boot-only keyboard entry or Display Off
saved. It requires a working OLED. A lever held during startup must be released
before it operates. Contacts are debounced for 20 ms; simultaneous contacts
are ignored until the lever returns to neutral. RECOVERY remains a separate
hardware reset/BOOTSEL button. Keyboard menu controls continue to work.

REV7 firmware and lever operation still need testing on the assembled board;
host tests and successful builds do not establish physical operation.

### Settings and input capture

While the menu is open, keyboard, mouse and joystick input is captured locally.
Previously forwarded keys, buttons and directions are released, and queued mouse
motion is discarded. After closing, release held keys/buttons and centre the
gamepad before using them again. The firmware
version stays visible, and normal status/pairing text is restored on exit.

| Setting | Choices | Default |
| --- | --- | --- |
| Menu key | F12, F11, Application/Menu | F12 |
| Menu entry | Hold for one second, boot only | Hold |
| Right GUI key | Right Amiga, hold to open menu, disabled | Right Amiga |
| Mouse wheel | TankMouse/Cocolino, off | TankMouse/Cocolino |
| Wheel direction | Normal, reverse vertical | Normal |
| Mouse step interval | 300, 200, 150, 100 microseconds | 300 |
| Display | Status, HID diagnostics, mouse diagnostics, off | Status |
| Watchdog | Off, 2 seconds, 5 seconds | Off |
| Controller port 1 | Mouse, Joystick, CD32 pad | Mouse |
| Controller port 2 (REV5/REV6/REV7) | Off, Joystick, CD32 pad | Joystick |

The selected menu key is reserved for configuration while a working OLED is
present. The Right GUI setting affects the right Windows/Command modifier;
Application/Menu retains its existing Right Amiga mapping unless selected as
the menu key. The default Amiga reset chord is unchanged.

On REV5/REV6/REV7, mouse on port 1 and joystick on port 2 work simultaneously by default.
Joystick uses a supported USB or Bluetooth HID controller's stick/D-pad and
up to three fire buttons. See [joystick setup](joystick.md) for mappings and compatibility.
Port 2 can be disabled with its own setting. Selecting Joystick on port 1
disables mouse input; if both ports are set to joystick, they mirror the same
combined controller state. Keyboard input continues normally in all modes.
REV2/REV4 builds expose only the port 1 setting and do not drive port 2.
Release the gamepad controls and mouse buttons after changing modes. See
[joystick mode](joystick.md) for supported formats and testing instructions.

Faster mouse intervals change the rate at which queued movement is emitted,
not mouse sensitivity. They require testing with the connected Amiga. The
watchdog restarts the Pico if its main loop stops completing; it does not
independently detect a stalled mouse core. It starts after normal initialization
and pauses when a debugger halts the CPU. The OLED and UART report "Watchdog
reboot" when the previous reset was caused by the watchdog; this message does
not mean the watchdog is currently enabled.

HID diagnostics show mount/receive status on the third line. With a supported USB
gamepad connected, the third and fourth lines show its report count, packet length,
decode result and routed controls; see [joystick diagnostics](joystick.md#diagnosing-missing-input).
Mouse diagnostics
update the fourth line at most ten times per second; Bluetooth pairing messages
take precedence. Turning the display off blanks normal status, but the menu
still appears when invoked. A missing or failed OLED prevents menu capture, so
normal input continues. Right GUI falls back to Right Amiga if it was assigned
to an unavailable menu; an explicitly disabled Right GUI remains disabled.

## Recover settings

During the **first ten seconds of input servicing after startup**, F12 always
opens the menu, even if another menu key or boot-only entry was saved.

For recovery, hold **F12 and Esc together for two seconds** during that window.
This applies factory defaults in RAM, disables the optional watchdog, enables
the display, and opens the menu. Select **Save and exit** to retain the defaults
across power cycles. Bluetooth pairing data is preserved. A connected USB
keyboard is the most direct way to enter this recovery mode at startup.

Invalid or missing saved settings automatically use defaults. If the display
itself has failed, the firmware continues using saved settings without a menu.

## Saving settings

Settings are saved in two alternating flash records. A failed save leaves the
menu open and does not apply the edited values. Saving unchanged settings does
not erase flash.

Normal UF2 firmware updates preserve saved settings and Bluetooth pairing data;
erasing the entire flash removes them. Compatible older settings are migrated
when loaded and written in the current format on the next explicit save.
Invalid or incompatible records fall back to another valid record or defaults.
If mouse input is unavailable after an update, check that port 1 is set to Mouse.

REV7 reserves the last 20 KiB of its 4 MiB flash: two settings sectors, two
Bluetooth sectors, and the final sector for the RP2350 SDK workaround. USB-only
and Bluetooth REV7 builds use the same locations. RP2040 builds retain their
existing 16 KiB reservation and settings locations.

Startup recovery and watchdog-triggered restarts still require hardware
validation.

## Bluetooth controls

Pico W Bluetooth builds provide **Bluetooth: On/Off** and **BT pairing policy**.
Save and exit applies the radio setting without rebooting. Off releases connected
inputs; saved bonds remain available when it is turned on again. Automatic is the
existing discovery/pairing behavior. Paired devices only accepts stored Classic
keys and resolves LE addresses against the bond database, including private addresses.

**Pair for 2 minutes** temporarily allows new devices in paired-only mode. It is
an immediate action using the saved radio setting; enable and save Bluetooth first.
Existing connections remain active. **Forget BT devices** requires Enter twice,
disconnects devices, clears Classic and LE bonds, and restarts Bluetooth. In
paired-only mode, use Pair afterwards to enrol devices again. These actions do
not save unrelated menu edits. USB-only builds omit these menu items.

Bluetooth radio and pairing-policy controls still require hardware validation.

## Keyboard mappings

**Keyboard preset** keeps the original QWERTY mapping, swaps Y/Z, or swaps A/Q
and W/Z. These are physical letter-key permutations, not full national AmigaOS
keymaps; select the matching locale on the Amiga for punctuation and accents.
**ISO extra key** can route the key beside left Shift to the Amiga ISO key rather
than the original backslash mapping. **Amiga Help key** selects Insert (default),
Home, Page Up, F11, or None; **Amiga Delete key** selects Delete (default),
Backspace, or None. Moving Help/Delete removes their old Insert/Delete bindings.
A key reserved for entering the menu continues to open the menu.

Eight custom slots override the preset and Help/Delete choices. Select a slot,
select **Custom source key**, press Enter, then press the source key. Escape
cancels learning. Use **Custom Amiga key** to choose the destination with Left/Right,
or Disabled to suppress it. **Clear custom slot** restores the normal mapping.
Save and exit applies and stores all changes. Source keys exclude modifiers and
reserved menu/recovery keys; destination choices include Amiga modifiers. Right
GUI keeps its separate setting and the default Ctrl-Amiga-Amiga chord is preserved.
Two keys or keyboards mapped to the same Amiga key retain it until both release.

Custom keyboard settings still require hardware validation.

## CD32 pad mode

Select CD32 pad in the controller-port menu to enable seven-button output.
Port 2 defaults to ordinary Joystick on REV5. See
[CD32 setup, mappings and compatibility](joystick.md#cd32-pad-mode).

## Amiga-side preferences

REV5/REV6/REV7 provide **Amiga preferences** beside the controller-port settings. Save or
cancel pending OLED edits before enabling it. The action temporarily hands port
2 to the native Workbench utility; port 1 mouse and keyboard remain available.
Hold F12 to cancel, or close the utility to restore controller output.
See [AmigaHIDPrefs setup](amiga-preferences.md).
