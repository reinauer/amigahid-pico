# AmigaHID Pico r6 — prototype hardware

Open `amigahid-pico.kicad_pro` with KiCad 10. This directory was copied from
`../kicad`; the original r5 files and existing firmware edits are untouched.
The schematic and PCB in this directory are the r6 design, not fabrication-proven hardware.

## Implemented

- Pico **W** (RP2040, LCSC C7203003) fitted by default; also supports Pico **2 W**
  (RP2350) on the same footprint. Both modules provide Wi-Fi/Bluetooth. A bare RP2350 + RM2 design is
  deferred; it requires a separate MCU power/flash/clock/RF layout.
- Two **SN74CBTD3861DBQR** 10-channel bus switches replace all 17 BSS138 level
  shifters. Ten isolated 4 × 10 kΩ resistor arrays replace 34 individual
  pull-ups; unused resistor channels and switch channels are intentionally NC.
  These are the **CBTD** parts, powered from 5 V; CBTLV3861 is not a substitute.
- Two PCB-mounted **female DE9** connectors, numbered by actual DE9 pins rather
  than the old IDC/ribbon interleave. They connect to the Amiga's male ports.
- A **6-pin mini-DIN A4000 keyboard link**, for the user's straight-through
  PS/2-to-PS/2 cable and A4000-to-A3000 mini-DIN/DIN adapter. The signalling is
  Amiga keyboard protocol. This is not a PC PS/2 keyboard input.
- A four-pin female OLED socket, **3V3 / GND / SCL / SDA**, matching the supplied
  SH1106 module pin table. GPIO2 is SDA; GPIO3 is SCL.
- Four 3.2 mm carrier holes for M3 nylon standoffs on **30.50 × 28.60 mm** centres.
  The OLED outline is **35.50 × 33.70 mm**, from the supplied mechanical drawing.
- One USB-A host port. Full underside Pico W USB pads replace the old board
  crevice / half-via attachment. GPIO14/15 are exposed at J8 for a future PIO-USB
  experiment; the USB-A port currently uses the native USB controller.
- External 5 V input and a power-source selector, plus a 500 mA hold PTC on the
  USB host branch. An external powered USB hub provides multiple device ports.

Two additional case mounts, H5 at (2.70, 63.00) mm and H6 at (53.30, 22.50) mm,
use **2.2 mm holes for M2 nylon screws/standoffs**. Coordinates are measured from
the top-left board corner. Use hardware no wider than 4.4 mm at the PCB; M3
hardware crowds the Pico. H1–H4 remain the OLED mounts.

The carrier has a **14 × 9 mm antenna notch** from x=10 to 24 mm and y=59 to
68 mm, open to the bottom edge. This removes the carrier substrate beneath the
antenna, following the module footprint’s suggested board edge. Keep case metal
and mounting hardware out of that antenna space.

Board bounding outline: **70 × 68 mm**, two copper layers. The bounding rectangle is about 6.4% greater area
than the original outline's 61.772 × 72.443 mm bounding box. The level-shifting
section is smaller; the direct connectors and OLED mounting determine the
carrier size. Three onboard USB sockets and an onboard hub are not included.

## LCSC sourcing

`SOURCING.csv` records the selected manufacturer part numbers, live LCSC stock
counts, check timestamps, datasheet links, and footprint checks. The schematic
and PCB use only **LCSC Part #**, the field recognized by the Fabrication Toolkit
plugin. `production/bom.csv` uses the same **LCSC Part #** column. LCSC stock does not itself guarantee JLC assembly stock
or eligibility; check those when uploading the BOM.

All 32 electrical components have stocked LCSC selections. **U1 is Pico W,
C7203003**, selected after checking both official pinouts and SMT drawings.
Pico 2 W is an optional separately sourced alternative. Use firmware built for
`pico_w` with the fitted RP2040 module or `pico2_w` with the RP2350 alternative. The OLED, standoffs, cables, and JP1 shunt are separate assembly
accessories; mounting holes are not purchasable components.

- U1: C7203003, Raspberry Pi Pico W. See `review/sourcing/pico-compatibility.txt`
  for the underside-pad and shell-contact verification.
- U2/U3: C44508, TI SN74CBTD3861DBQR (SSOP-24, 0.635 mm pitch). This replaces
  the initially selected PWR/TSSOP package; the electrical pin assignments are
  unchanged. Use the updated PCB, not the previous TSSOP fabrication files.
- J7: C7428703, HOOYA DIN-603. The six signal holes, 13.9 mm shield spacing,
  central 3 × 1 mm shield slot, and signal numbering match the local footprint.
  Numbering was cross-checked against LCSC’s EasyEDA model as well as the drawing.
- J5/J6: C50380980, LCK-205A09FJ2A00B1A. Signal drills are 1.10 mm to meet the
  supplier’s 1.09 mm hole recommendation; 25 mm mounting-hole pitch is retained.
- J4: C7279912, JST UBA-4R-D14-4D(LF)(SN), matching the existing USB signal and
  shield-hole pattern. USB routing is retained.
- Connector 3D bodies are illustrative, not exact supplier mechanical models.

`review/sourcing/stock-snapshot.json` preserves the stock evidence. Stock counts
are a dated snapshot, not a reservation or an ongoing availability guarantee.

Assembly rotation corrections use `FT Rotation Offset` for Fabrication Toolkit:
U1 = 270°, C4 = 180°, J7 = 0°, and J1–J6/J8/J9/JP1 = 90°.
These offsets incorporate the assembly-preview corrections: U1 and C4 turned
180° from the preceding export, and J7 turned 90° clockwise. They affect only
exported placement rotation. Review pin 1, C4 polarity and connector openings
in the JLC preview after re-uploading the CPL.

## Power and assembly

**Default: regulated 5 V into J9, with a 2 mm pitch shunt on JP1 pins 2–3.**
J9 pin 1 is +5 V input; pin 2 is ground. Supply at least the measured Pico +
peripheral load; a current-limited 5 V bench supply is appropriate for bring-up.
The selector keeps the Amiga's keyboard +5 V separate in external-power mode.
Do not bridge both selector positions. Grounds remain common. Power the adapter and Amiga together; automatic I/O
isolation when either supply is off is not implemented and needs bench review.

JP1 pins 1–2 select power from the Amiga keyboard connector. Use that option
only after checking the actual machine's port budget; the complete 500 mA USB
branch must not be assumed available from a keyboard port. The PTC is overload
protection, not a precise 500 mA current regulator or a USB power-distribution
switch. Use a powered hub for multiple USB loads. DE9 pin 7 is deliberately NC
on both ports, so mouse-port supplies are not paralleled with keyboard power.

D1 feeds Pico VSYS through a Schottky diode, preventing the Pico's programming
USB supply from feeding the selected board rail. Disconnect host devices when
connecting a PC to the Pico's native USB connector: the two connectors share
D+/D-. Do not connect two USB hosts to those data lines.

The Pico module must be **surface mounted/reflowed**, including TP2 and TP3;
plugging it into ordinary 40-pin sockets does not connect the USB test pads.
The local Pico footprint retains the wireless antenna keepouts and omits only
two paste-only apertures which conflicted with its RF keepout. No connector,
OLED metalwork, ground fill or trace should extend into the antenna keepout.

The OLED pin row starts 11.28 mm to the right of the upper-left mounting-hole
centre and 1.54 mm below its PCB top edge, following the supplied drawing.
Use a matching four-pin female socket on the carrier. Approximately 11 mm
board-to-board spacing suits an 8.5 mm socket plus a 2.54 mm male-header body;
**dry-fit the actual supplied header and standoffs before assembly**. All four
OLED holes are part of the carrier; the module outline is on Dwgs.User.
Use nylon spacers to avoid shorting OLED tracks. The display sits above the
carrier components; confirm connector insertion clearance in the assembled stack.

## Connector pinouts

| Connector | Pins |
|---|---|
| J1 A500 | 1 KCLK, 2 KDAT, 3 /RESET, 4 Amiga +5 V, 5 NC, 6 GND, 7 NC, 8 NC |
| J2 UART/SWD | 1 GND, 2 GPIO0/TX, 3 GPIO1/RX, 4 SWDIO, 5 GND, 6 SWCLK |
| J3 OLED | 1 3V3, 2 GND, 3 SCL/GPIO3, 4 SDA/GPIO2 |
| J5/J6 DE9 | 1 Up, 2 Down, 3 Left, 4 Right, 5 button 3, 6 Fire, 7 NC, 8 GND, 9 button 2 |
| J7 A4000 | 1 KDAT, 2 NC, 3 GND, 4 Amiga +5 V, 5 KCLK, 6 NC; shell GND |
| J8 experiment | 1 GND, 2 GPIO14, 3 GPIO15, 4 3V3; no USB firmware or 5 V power at this header |
| J9 external supply | 1 regulated +5 V, 2 GND |
| JP1 2 mm selector | 1 Amiga +5 V, 2 board +5 V, 3 external +5 V; fit one shunt, default 2–3 |

Check the PS/2 cable for **pin-for-pin continuity**, especially 1, 3, 4 and 5.
The A3000 adapter must map A4000 mini-DIN data/clock/ground/supply to the matching
A3000 DIN signals. Do not infer numbering from the mating face or wire colours.
The mini-DIN does not carry A500 /RESET; J1 retains that signal.

## Firmware compatibility and remaining validation

Amiga GPIO assignments retain `BOARD_HIDPICO_REV5`, including the corrected
port-2 GPIO26/GPIO27 mapping. Firmware must drive the Amiga outputs LOW or
release them; the existing keyboard and controller drivers already do that.
The bus switches do not actively generate a 5 V HIGH; the 5 V pull-ups do.

A local Pico 2 W + Bluetooth build was attempted and failed at `src/settings.c:41`:
`Settings overlap Bluetooth pairing storage`. The installed picotool is also
2.2.0-a4 while this SDK requires 2.3.0; the compile check used
`PICO_NO_PICOTOOL=ON`, so it could not produce a UF2 even if compilation passed.
Resolve the flash partition configuration before treating RP2350 firmware as
ready. Logs are in `review/firmware-*.log`.

Pico 2 W requires a `pico2_w`/RP2350 firmware build with Bluetooth enabled;
Pico W requires `pico_w`/RP2040. A wireless footprint alone does not enable
Bluetooth in a firmware image. Existing firmware source is not changed here.

**The current display driver is SSD1306, not SH1106.** Despite the seller's
compatibility wording, SH1106 needs page-addressed writes, its DC/DC commands,
a typical two-column RAM offset and an I2C speed no higher than its specified
400 kHz. The current 1 MHz SSD1306 horizontal-addressing driver must be adapted
before the requested display can work. This hardware provides the correct
supply, signals and mounting, but does not claim SH1106 software support.

Before promoting this prototype to a manufacturing release:

1. Dry-fit the selected connectors, OLED/socket/spacers and cables. Supplier
   footprint checks and the exact selected parts are recorded in `SOURCING.csv`.
2. Check continuity, diode polarity, power selector isolation and all connector
   pinouts without an Amiga attached. Verify 5 V supply and 3.3 V rail sequencing.
3. Scope LOW/HIGH levels on both sides of both CBTD3861s under actual Amiga
   pull-ups and cable loads, including reset, simultaneous transitions, startup
   and shutdown. TI specifies the part's operating supply as 4.5–5.5 V.
4. Check the Pico pins for overshoot/back-powering in every supply state. The
   CBTD3861's transfer curve is not an absolute overvoltage clamp guarantee.
5. Test USB enumeration/hotplug with a keyboard, mouse and joystick through a
   powered hub. Check inrush, PTC heating, supply drop and USB waveform quality.
   The manually routed pair is matched to below 0.3 mm; this alone is not USB
   compliance or a verified 90-ohm impedance calculation for a chosen fab stackup.
6. Verify Bluetooth operation and RF range with the display and enclosure fitted.
7. Implement/test SH1106 firmware and verify the image reaches all display edges.

No remote release/tag is published and no fabrication order is placed.

## Files and reproducibility

- `amigahid-pico.kicad_pro`, `.kicad_sch`, `.kicad_pcb`: editable r6 project.
- `SOURCING.csv`: reviewed electrical component list with `LCSC Part #` fields.
  Run `python3 scripts/sync_sourcing.py` to generate `BOM.csv`; add OLED, a 2 mm shunt,
  four M3 nylon spacers and mating power/display connectors to the assembly order.
- `review/oled-*.png` and `review/sourcing/`: supplied mechanical drawings and
  sourcing evidence. Downloadable supplier PDFs are linked in `SOURCING.csv`.
  Exported schematics, board previews, validation reports, Gerbers and production
  files are generated locally and excluded from Git.
- `scripts/sync_sourcing.py`: reapplies reviewed `SOURCING.csv` metadata to the
  schematic, PCB and BOMs without changing the circuit or layout. It does not
  refresh live stock automatically.
- `scripts/validate_r6.py`: repeat the connectivity, mechanics, ERC and DRC checks
  using `/usr/bin/python3 scripts/validate_r6.py` from this directory.
- Local footprint/symbol libraries are project-relative. Only the four r6
  footprints and the required mini-DIN model are tracked. KiCad 10 supplies the
  remaining standard footprints and models. r5 references, caches and one-off
  placement/routing generators are excluded from Git.

## Sources

- [TI SN74CBTD3861 product page](https://www.ti.com/product/SN74CBTD3861)
  and [datasheet](https://www.ti.com/lit/ds/symlink/sn74cbtd3861.pdf).
- [Raspberry Pi Pico 2 W datasheet](https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf).
- [Raspberry Pi RM2 documentation](https://www.raspberrypi.com/documentation/microcontrollers/radio-modules.html).
- [Commodore A4000 schematic, CN175](https://www.amigawiki.org/dnl/schematics/A4000_Rb.pdf).
- [Sino Wealth SH1106 datasheet](https://www.crystalfontz.com/controllers/SinoWealth/SH1106/468/).
- OLED dimensional drawing and pin table supplied by the user in this session;
  saved under `review/` with the original dimensions visible.

Original project author: nine <nine@aphlor.org>. See the repository README for
the project's Eclipse Public License 2.0 licensing statement.
