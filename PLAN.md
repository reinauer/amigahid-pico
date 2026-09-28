# Implementation status: features branch (2026-09-27)

Based on `bluetooth` at `b437643`. The existing Bluetooth PR is unchanged.

Implemented in the first runtime-configuration milestone:

- Versioned settings with CRC validation, defaults, and two alternating flash
  records. Settings storage is separate from Bluetooth pairing data.
- OLED settings menu entered by holding F12 for one second; arrow navigation,
  save, cancel, and factory defaults. Input is captured while the menu is open.
- Menu key/entry policy, Right GUI behavior, wheel enable/direction,
  quadrature timing presets, display diagnostics/off, and optional watchdog.
- Fixed boot recovery: hold F12+Esc for two seconds during the first ten seconds
  of input servicing. Defaults are applied in RAM; save explicitly to keep them.
- The watchdog feeds only after the main-loop services return; it is off by
  default. USB-only builds also initialize core1's flash lockout support.

Hardware check on Pico W/REV5 (initially using the REV4 firmware): the menu works,
and a saved 5-second watchdog setting survives power loss. Recovery and an actual watchdog-triggered restart
still need hardware validation.

Usage and validation notes: [doc/runtime-configuration.md](doc/runtime-configuration.md).

Implemented in the next digital-joystick milestone; hardware validation pending:

- USB HID gamepad/joystick hat switches, discrete D-pad inputs and absolute X/Y
  fields, with Button 1 as fire. Reports are decoded from their descriptors,
  including report IDs. X/Y uses a central deadzone and digital output.
- A dedicated REV5 build defaults to mouse on controller port 1 and USB joystick
  on port 2 simultaneously. Port 1 can still select joystick; port 2 has an Off
  option. REV2/REV4 retain port 1 only. Core1 owns output pins; menu entry, mode
  changes and disconnects release controls. Opposing directions cancel.
- Settings schema 3 migrates schema 1 and 2, preserving existing settings and
  enabling the new port 2 option by default on REV5. A saved port 1 joystick
  selection is retained; change it to Mouse for the two-port arrangement.

Usage and supported formats: [doc/joystick.md](doc/joystick.md).

Competition Pro Extra (`0079:181c`) input checked on Linux: its digital stick uses
X/Y values 0/128/255, with the advertised hat unused. Native decoding checks pass
using the actual descriptor and captured axis values. All four buttons produced
events. Hardware testing detects the controller, but Amiga Test Kit shows no
activity on either port, including port 1 through the working mouse cable.
HID diagnostics now expose USB report count/length, decoding, routed controls
and waiting for neutral to locate the failure; the cause is not yet confirmed.

Still to implement or validate:

- Measure input latency and validate the faster mouse timing presets on hardware.
- Earlier HID rearming and GPIO timing instrumentation.
- Keyboard PIO; consider mouse PIO only after measurement.
- Validate USB digital joystick output on hardware; add configurable deadzones
  and button mappings, Bluetooth gamepads, keyboard-as-joystick, and CD32 output.
- Full key remapping/layout presets, further controller modes, Bluetooth configuration,
  and the optional Amiga preference tool.

The sections below retain the design and acceptance criteria for these phases.

# PIO Offload Plan

## Goal

Reduce end-to-end input latency without regressing keyboard or mouse
reliability on the Amiga.

## Current State

- USB host processing runs on `core0`.
- Keyboard state updates still happen on `core0`, but the blocking
  Amiga wire transmission is now queued out of the HID callback path.
- The current keyboard sender still bit-bangs the Amiga serial protocol,
  but that work is now serviced from the normal event loop instead of
  stalling USB report handling directly.
- Mouse quadrature output already runs on `core1`.
- The current mouse path uses a 64-bit deadline with a default 300 us step
  interval. The features branch makes that interval configurable; faster
  presets still need hardware validation.

## Priority

1. Tune or redesign mouse pacing.
2. Measure the result.
3. Shorten the HID hot path further if needed.
4. Only implement mouse PIO if measurements still justify it.

## Phase 1: Measure Baseline

- Add simple timing instrumentation for:
  - USB report received
  - keyboard send start and end
  - first mouse quadrature edge after a report
- Compare:
  - direct USB mouse and keyboard
  - the same devices through the KVM
- Record:
  - typical latency
  - worst-case latency
  - burst behavior

## Phase 2: Decouple Keyboard Sending

Completed.

- Stop sending Amiga keyboard codes directly from the HID callback path.
- Add a small queue for translated Amiga key events.
- Keep key translation, modifier tracking and caps lock handling on
  `core0`.
- Move the actual wire-level transmission out of the synchronous USB
  path.

## Phase 3: Implement Keyboard PIO

- Replace the current blocking keyboard bit-bang transmitter with a
  PIO-backed sender.
- Feed pre-encoded key events into a PIO state machine from software.
- Preserve current behavior for:
  - key up and key down encoding
  - caps lock special handling
  - Ctrl-Amiga-Amiga reset behavior
- Keep a software fallback until the PIO path is validated on hardware.

## Phase 4: Reevaluate Mouse Latency

- Re-test after the keyboard work lands.
- Measure whether the remaining mouse lag is dominated by:
  - KVM polling or buffering
  - the `motion_divider`
  - the fixed `300 us` quadrature pacing
- If needed, make the quadrature step delay configurable and test lower
  values before attempting a larger rewrite.

## Phase 5: Mouse PIO Only If Needed

- Consider mouse PIO only if measurements show that the current
  `core1` quadrature loop is still a significant source of latency or
  jitter.
- A mouse PIO implementation should focus on:
  - precise quadrature timing
  - higher sustained output rate
  - lower burst drain time
- Do not treat mouse PIO as the first optimization target, since the
  current mouse path already avoids blocking USB handling on `core0`.

## Acceptance Criteria

- Keyboard transmission no longer blocks USB event handling for several
  milliseconds per keycode.
- No regressions in key mapping, caps lock behavior or reset chord
  handling.
- Mouse movement remains correct on both axes with no button regressions.
- Measured latency improves in a way that is noticeable on hardware, not
  just in code structure.

## Status Against Earlier Latency Findings

Already addressed in the current tree:

- The old cross-core mouse handoff race has been replaced with a queue.
- Core1 now re-merges newly queued motion between quadrature steps.
- Per-report mouse debug is disabled by default and only enabled when
  `DEBUG_MOUSE` is set.
- Blocking Amiga keyboard transmission has been moved off the USB HID
  callback path into a queue serviced from `amiga_service()`.

Still open:

- Hardware validation of the configurable quadrature timing presets
- HID callback still rearms after handling the report
- No built-in latency instrumentation

## Remaining Work With Estimated Latency Impact

### 1. Make Quadrature Step Timing Configurable

- Implemented on features: select a 300, 200, 150, or 100 us step interval.
  The 300 us default is unchanged; faster presets need hardware testing.
- Test lower values such as `200 us`, `150 us` and `100 us`.
- Choose the lowest value that still tracks reliably on real hardware.

Estimated latency reduction:

- tiny movements: roughly `0-1 ms`
- medium bursts: roughly `3-8 ms`
- `50`-step bursts: roughly `7.5-10 ms`
- very large bursts: up to roughly `20 ms`

This is the highest-value remaining item for pure mouse feel.

### 2. Rearm HID Reports Earlier In The Hot Path

- Copy the essential report data quickly.
- Re-request the next HID report before doing lower-priority work where
  practical.

Estimated latency reduction:

- typical cases: roughly `0-0.2 ms`
- worst-case jitter under load: roughly `0.5-1 ms`

This is now a cleanup improvement rather than the main latency win.

### 3. Add GPIO Timing Instrumentation

- Toggle one GPIO on USB report receipt.
- Toggle another on first emitted quadrature edge.
- Compare direct USB devices against the same devices through the KVM.

Estimated latency reduction:

- direct reduction: `0 ms`

This does not improve latency by itself, but it is the fastest way to
separate KVM delay from firmware delay and validate whether the
remaining work is worth doing.

### 4. Implement Keyboard PIO As A Follow-Up

- Replace the queued bit-bang sender with a PIO-backed transmit path.
- Keep the queue-based architecture, but hand the actual waveform
  generation to a state machine.

Estimated latency reduction:

- USB callback blocking: roughly `0 ms` additional reduction, since the
  main callback decoupling is already done
- keyboard queue drain time under heavy typing: potentially `1-5 ms`
- jitter during mixed input under sustained key bursts: modest but real

This is still worth doing, but it is no longer the first latency item to
attack.

# Joystick Emulation Plan

## Goal

Add joystick and joypad emulation on top of the existing HID input
paths, starting with the simplest and most compatible target.

## Recommended Order

1. HID joystick and gamepad to Atari-style digital joystick
2. Keyboard-to-joystick as an explicit special mode
3. CD32 pad emulation

## Why This Order

- Plain digital joystick is the simplest controller-port target and
  reuses the same physical Amiga port lines already driven for mouse
  emulation.
- Keyboard-to-joystick is useful, but it should be an explicit mode so
  normal typing is not affected.
- CD32 pad support is more complex because the extra buttons are not
  just plain digital joystick inputs and need separate handling.

## Phase 1: HID Joystick to Digital Joystick

USB hat/D-pad, absolute X/Y and Button 1 support is implemented on `features`;
Pico-to-Amiga hardware testing and configurable thresholds remain outstanding.
See [doc/joystick.md](doc/joystick.md).

- Add a controller backend that drives one Amiga controller port as:
  - up
  - down
  - left
  - right
  - fire
- Parse USB HID joystick and gamepad reports into that state.
- Support D-pad inputs first.
- Add analog stick support after that with deadzones and axis-to-digital
  thresholding.
- Keep the first implementation to one emulated joystick port unless
  the board pin mapping is expanded.

## Phase 2: Keyboard as Joystick Mode

- Add a dedicated mode that maps selected keys to joystick directions
  and fire buttons.
- Start with a sensible default mapping such as:
  - WASD for directions
  - space and ctrl for fire
- Make the feature opt-in so those keys still work normally in standard
  keyboard mode.
- Implement it as a virtual controller source, not as special cases in
  the normal keyboard path.

## Phase 3: CD32 Pad Emulation

- Add a separate backend for CD32-compatible controller output.
- Reuse the basic digital directions from the earlier joystick backend.
- Implement the extra CD32 button handling after the plain joystick path
  is stable.
- Keep this work isolated from the first joystick milestone so the
  simpler one-button case can ship earlier.

## Design Notes

- Reuse the shared input bridge pattern where it helps, so USB and
  Bluetooth controllers can feed the same Amiga-side output path.
- Keep joystick output independent from mouse output, even though they
  share the same Amiga controller connector pins.
- Provide a simple way to select the active emulation mode for a port:
  - mouse
  - joystick
  - keyboard as joystick
  - later, CD32 pad

## Acceptance Criteria

- A standard USB or Bluetooth gamepad can control Amiga software that
  expects a plain digital joystick.
- Keyboard-as-joystick mode works without leaking those keys into normal
  keyboard input when the mode is enabled.
- The initial joystick implementation is stable on hardware and does not
  regress existing mouse emulation.
- CD32 support remains a separate follow-up until the basic joystick
  path is proven.

# Runtime Configuration Plan

## Goal

Move user-facing configuration out of build-time defines where practical
so hardware can be assembled, fitted and adjusted without rebuilding
firmware for small behavioral changes.

## Motivation

- Some choices are annoying to validate only at build time, such as key
  remaps and mode selection.
- A misconfigured modifier or special key can block important machine
  functions such as the Amiga reset chord.
- The board already has an OLED on supported revisions, which makes a
  simple local configuration UI realistic.
- Settings should persist across power cycles and allow recovery to
  defaults.

## Scope

- Keep hardware-specific pin mapping as a build-time concern unless a
  safe runtime board-detection mechanism is added later.
- Move behavioral settings to runtime where possible, such as:
  - HID to Amiga key remaps
  - menu key versus right GUI behavior
  - mouse wheel mode and direction
  - mouse quadrature timing preset
  - port mode selection
  - joystick mode options
  - debug display preferences
  - Bluetooth behavior that does not affect low-level bring-up
- Keep hardware-specific and low-level electrical choices as build-time
  settings:
  - board type
  - GPIO pin mapping
  - PIO allocation details
  - port electrical assumptions

## Phase 1: Define Settings Model

- Add a single settings structure with:
  - version
  - checksum or CRC
  - defaults marker
  - behavioral options only
- Define factory defaults in one place.
- Keep the initial schema small and focused on the highest-value v1
  settings:
  - menu entry key, defaulting to F12
  - menu entry behavior, such as long-press or boot-only
  - right GUI behavior:
    - pass through as Right-Amiga
    - use as menu key
    - disable
  - mouse wheel mode:
    - off
    - TankMouse/Cocolino
  - mouse wheel direction:
    - normal
    - vertical reverse
  - mouse quadrature timing preset
  - debug display mode:
    - normal status
    - HID debug
    - mouse debug
    - off
  - watchdog enable and timeout preset
- Defer broader settings until the core storage and menu path are
  proven:
  - full custom key remapping
  - keyboard layout presets
  - joystick and gamepad mapping
  - Bluetooth pairing policy
  - CD32 pad mode

## Phase 2: Flash Persistence

- Reserve a flash region for configuration storage.
- Add load, validate and save helpers.
- On boot:
  - load settings from flash
  - validate version and checksum
  - fall back to defaults if invalid
- Add a reset-to-defaults path that can be triggered even if saved
  settings are broken.

## Phase 3: Runtime Menu

- Add a display-backed configuration menu for boards with `HAS_SCREEN`.
- Keep the first UI simple and robust:
  - current value
  - next and previous item
  - save
  - reset to defaults
- Avoid requiring a host computer or serial console for normal setup.
- If no display is present, continue to boot with saved settings and
  defer UI work for headless configuration until later.

## Phase 4: Input and Navigation

- Define how to enter the menu safely at boot or runtime.
- Use a key that exists on typical PC keyboards but is not forwarded as
  a normal Amiga key; F12 is the preferred default.
- Support long-press entry at runtime to avoid accidental activation.
- Support a boot-time entry path and a reset-to-defaults gesture so a
  broken saved configuration can be recovered.
- Choose a navigation method that does not require special hardware,
  such as:
  - keyboard keys
  - a connected mouse or controller
  - a held boot gesture
- Ensure the menu can always be exited without corrupting saved state.

## Phase 5: First Runtime-Configurable Features

- Start with settings that solve real deployment pain:
  - menu entry key and entry behavior
  - right-Amiga mapping choice
  - mouse wheel mode and vertical direction
  - mouse quadrature timing preset
  - optional debug screen toggles
  - watchdog enable and timeout choice
- Keep each setting independent and easy to reset.

## Phase 5a: Follow-Up Runtime-Configurable Features

- Add these after the settings model, flash storage and menu UI are
  stable:
  - controller port mode selection:
    - mouse
    - digital joystick
    - auto, if detection is reliable enough
  - keyboard-as-joystick enable and keymap preset
  - gamepad mapping:
    - D-pad versus analog stick
    - fire button mapping
    - analog thresholds and deadzones
  - Help/Delete mapping choices for different PC keyboard layouts
  - keyboard layout preset
  - Bluetooth enable, pairing mode and forget-paired-devices action
  - CD32 pad mode, once CD32 output is implemented
- Avoid starting with a full arbitrary key remapper; it is useful, but
  it will make the first settings schema and UI much larger.

## Phase 6: Migration and Safety

- Add schema versioning so future firmware can migrate old settings.
- Protect against partial writes and invalid flash contents.
- Make the default path conservative so a bad config does not leave the
  device unusable.
- Document a physical or boot-time recovery path for resetting config.

## Phase 7: Watchdog Support

- Add RP2040 watchdog support around the normal main event loop.
- Feed the watchdog only when the system is making forward progress in
  the expected loop.
- Choose a timeout that tolerates normal long operations but still
  recovers from real hangs.
- Record the last reboot reason where practical so watchdog resets are
  distinguishable from power-on resets.
- Make watchdog support configurable at runtime and default it to the
  behavior that proves least surprising in practice.
- Allow the watchdog to be disabled from saved settings because forced
  reboot behavior may be irritating during development or debugging.

## Watchdog Design Notes

- Do not feed the watchdog from many unrelated places, or it will stop
  detecting the actual main-loop stalls we care about.
- Keep the watchdog decision in the runtime settings model rather than a
  separate ad-hoc switch.
- Ensure there is still a recovery path if a saved watchdog setting is
  undesirable, such as booting into the menu or resetting config to
  defaults.

## Acceptance Criteria

- A user can change common behavioral settings without rebuilding the
  firmware.
- Settings persist across power cycles.
- Invalid or missing settings cleanly fall back to defaults.
- A broken configuration can be reset without external tools.
- The runtime menu does not interfere with normal input handling when it
  is not active.
- Watchdog behavior can be enabled or disabled without rebuilding the
  firmware.

## Experimental Follow-Up: Amiga Preference Panel

- Consider a native Amiga-side preference tool as a later extension of
  the runtime configuration work.
- This would use the controller-port button lines as a low-speed
  bidirectional signaling path only while an explicit configuration
  session is active.
- Do not treat this as the primary configuration path. The OLED menu and
  local runtime settings remain the first-class solution.

### Preconditions

- A reliable local runtime configuration system must exist first.
- The communication mode must not interfere with normal mouse, joystick
  or future CD32 behavior.
- There must be a clear way to enter and leave communication mode.

### Protocol Requirements

- Add an attention or wake-up signal that is unlikely to be triggered by
  normal input activity.
- Use a small half-duplex protocol with:
  - framing
  - versioning
  - checksum or CRC
  - timeout and retry behavior
- Abort cleanly back to normal port behavior if negotiation fails.

### Safety Requirements

- Keep the feature disabled unless explicitly invoked.
- Ensure failed communication cannot leave the controller port stuck in a
  non-input mode.
- Preserve a fully local recovery path through the OLED menu or reset to
  defaults even if the Amiga-side tool is unavailable.

### Scope

- Read current settings
- Change selected runtime settings
- Save settings to flash
- Reset settings to defaults

### Non-Goals for the First Version

- Replacing the OLED menu
- Streaming normal input events over the config channel
- Combining config traffic with normal mouse or joystick emulation at
  the same time
