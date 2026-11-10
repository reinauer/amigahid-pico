# Firmware host checks

Run from the repository root:

```sh
sh tests/run-menu-tests.sh
python3 tests/check-r7-pins.py
```

The menu tests compile the actual contact state machine and runtime menu,
with mock GPIO, clock, display, storage and input-routing boundaries. They
cover debounce, repeat timing, startup-held and ambiguous contacts, long vs
short pushes, settings editing/saving/cancelling, key learning, keyboard
handover, display loss, Bluetooth confirmation and the Amiga preferences
link. Both Bluetooth and USB-only menu configurations run on the host.

The pin check compares the application and SDK board-header definitions
against `kicad_r7/circuit.json` and the MCU symbol's physical pin mapping.

These checks do not exercise the RP2350, electrical contacts, OLED, RM2,
flash controller or Amiga hardware. Build the firmware separately as
[documented](../doc/installation.md#rev7-rp2350a-and-rm2).
