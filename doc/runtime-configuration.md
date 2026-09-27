# Runtime configuration

The `features` branch adds a settings menu to the existing USB and Bluetooth
firmware. It works with the OLED on REV2 and REV4 boards. Board selection and
GPIO wiring remain build-time options.

## Open and use the menu

Hold **F12 for one second**, then release it. USB and Bluetooth keyboards use
the same menu. Press Up/Down to select an item, Left/Right to change its value,
and Enter to change a value or activate an action. **Save and exit** writes
settings to flash and applies them. **Esc** or **Cancel changes** discards edits.

Factory defaults changes the values in the editor; select Save and exit to
apply and preserve them. Values are not previewed while editing.

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
| Controller port | Mouse, USB joystick | Mouse |

The selected menu key is reserved for configuration while a working OLED is
present. The Right GUI setting affects the right Windows/Command modifier;
Application/Menu retains its existing Right Amiga mapping unless selected as
the menu key. The default Amiga reset chord is unchanged.

Controller port selects what the existing Amiga mouse/controller connector
emulates. USB joystick uses a supported USB HID controller's stick/D-pad and Button 1;
mouse input is inactive in that mode, while keyboard input continues normally.
Release the gamepad controls and mouse buttons after changing modes. See
[USB joystick mode](joystick.md) for supported formats and testing instructions.

Faster mouse intervals change the rate at which queued movement is emitted,
not mouse sensitivity. They require testing with the connected Amiga. The
watchdog restarts the Pico if its main loop stops completing; it does not
independently detect a stalled mouse core. It starts after normal initialization
and pauses when a debugger halts the CPU. A watchdog reboot is reported on the
OLED and UART.

HID diagnostics show mount/receive status on the third line. Mouse diagnostics
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

## Storage and validation

Settings use schema version 2 and a CRC32. Two alternating 4 KiB sectors keep
the previous valid record intact while a new record is erased/programmed. On
boot, the newest valid record is selected, including across sequence rollover.
Unchanged saves do not erase flash. Failed saves leave the menu open and do not
apply the edited values.

The last 16 KiB of flash is reserved by a linker assertion: two settings sectors
followed by the SDK's two Bluetooth pairing sectors. USB-only and Bluetooth
builds use the same layout. Settings writes run through the SDK's flash-safe
execution API with core1 registered for lockout. Normal UF2 updates below this
region preserve settings; erasing the entire flash removes them.

Schema 1 records from the first runtime-configuration firmware are migrated in
RAM, preserving all eight existing settings, including the watchdog timeout.
The new controller mode defaults to Mouse. The next explicit save writes schema
2. Unknown schemas are rejected, falling back to a compatible record or defaults.

Temporary native checks cover CRC/schema validation, interrupted writes and
erases, sequence rollover, schema migration, save/cancel, menu capture and held-key
release, recovery, display caching and watchdog control. The menu and persistence
of a saved 5-second watchdog timeout have been confirmed on Pico W/REV4 across
power loss. Recovery, an actual watchdog-triggered restart, and the new joystick
mode still need hardware validation. No new tests are checked into the repository.
