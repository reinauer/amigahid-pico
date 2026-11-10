# AmigaHID r7 — hardware prototype

Copied from `kicad_r6`. **Prepared for a factory quotation and prototype CAM
review; not released for manufacture.** The r6 project is unchanged.

## Current design

- **70 × 66 mm**, four copper layers, nominal 1 mm PCB. The bounding rectangle is
  2.9% smaller than r6's 70 × 68 mm board. All assembled components are on top.
  The DB9 edge extends 6 mm under the housings; KiCad Y now spans -6..60 mm,
  retaining all component and route coordinates.
- Bare RP2350A, 4 MB Winbond QSPI flash, and left-facing RMC20452T / RM2 radio.
- Centered 35.5 × 33.7 mm OLED, two DE9 ports, mini-DIN keyboard and two stacked
  USB-A ports. The CH334P hub uses two of its four downstream channels.
- USB-C and a side-actuated RECOVERY switch at the bottom right.
- H1–H4 are shared OLED/base/main-PCB mounts. H5/H6, the A500 header, extra power
  header and selector jumper are removed. UART/SWD use bare pads.
- Five-sheet schematic: interfaces, controller, power, USB, and control.

Open [the KiCad project](amigahid-pico.kicad_pro). The normal editable design is
[the schematic](amigahid-pico.kicad_sch) and [the PCB](amigahid-pico.kicad_pcb).
The matching enclosure is in [`../case_r7`](../case_r7/README.md).

## Power and recovery

Mini-DIN pin 4 remains a 5 V power input. The TPS2116 automatically gives
USB-C priority and falls back to the keyboard supply when USB-C is removed.
It has reverse-current blocking, so the two supplies are not directly joined.
See the [TPS2116 datasheet](https://www.ti.com/lit/ds/symlink/tps2116.pdf).
The DE9 5 V pins remain unconnected. No power-selection jumper is needed.

The [A4000 user guide, appendix B-8](https://device.report/m/e9fdb1a480863f0ee437f3aaf1a4cf073445989ed93d380fc9f8956629044f95.pdf#page=85)
rates keyboard power at **100 mA**. The user's current revision operates from
this port; r7 preserves that option, but its new MCU, hub, radio, display and
peripherals require a fresh current measurement. The 500 mA hold PTC is shared
by both USB-A ports and does not establish available Amiga current.

USB-C can also power normal operation from an adequately rated 5 V supply.
Use a supply rated for at least 1 A for prototype testing. This board does not
negotiate USB PD or read Type-C current advertisements. A PC's unconfigured
USB port is not a guaranteed normal-operation power source: hold RECOVERY
before connecting a PC for service. Keyboard-only operation must stay within
the actual Amiga/cable budget; adding USB-C power does not raise that rating.

| State | Action and result |
|---|---|
| Amiga off, PC recovery | Hold RECOVERY, connect USB-C, wait at least 0.1 s, release. |
| USB-C already connected | Press/release RECOVERY to enter ROM recovery. |
| Normal operation from USB-C supply | Connect power without holding RECOVERY. |
| Return to keyboard-powered operation | Remove USB-C, then press/release RECOVERY to reboot normally. |
| Return to USB-C-only normal operation | Unplug/reconnect USB-C without holding RECOVERY. |

Recovery is implemented in hardware. RECOVERY_N low presets the SN74HCS74 latch;
Q2 pulls QSPI-CS low through R6, U7 selects the PC data path, and U11 removes
USB-A power. The Amiga signal switches are disabled whenever keyboard power
is absent or recovery is selected. Thus invalid application flash does not
prevent button-based ROM recovery. This assumes normal RP2350 OTP settings;
permanently disabling ROM USB boot in OTP is outside this prototype design.

U13 (TPS3808G33DBVR) monitors the shared 3.3 V rail and holds RUN low
until the rail exceeds its nominal 3.07 V threshold and the reset-release
delay expires. CT is intentionally unconnected for a nominal 20 ms delay.
C41 provides local 100 nF bypassing; R12 remains the 10 kΩ RUN pull-up.
This allows the external flash to power up before the MCU starts its boot ROM.

The switch grounds RECOVERY_N, pulled up by R26 (10 kΩ). This net drives
both U13's manual-reset input and U9's preset input. U13's automatic reset
output drives RUN only, so cold-start and brownout resets do not themselves
request BOOTSEL. Button release also receives the supervisor's reset delay.
The recovery latch still retains the requested USB mode after release.

The latch uses Schmitt inputs and a separate 100 kΩ/100 nF clear delay. U12 translates
USB VBUS to 3.3 V with power-off protection. At power-up with the button held,
both asynchronous latch inputs can be asserted: the documented state has
both outputs high, while U13 holds RUN low to keep the MCU reset and D2 keeps USB-A power off.
Hold the button until the clear delay has expired, then release. The same
button without USB-C performs a normal reset. There is no firmware timing or
double-tap dependency.

U11 ramps the 220 µF host reservoir with 10 nF on CT (about 26.6 ms typical
10–90% rise at 5 V). Reservoir charging is about 33 mA over that interval,
before peripheral load and tolerances. Recovery leaves this branch off.
Direct USB-C bypass is 4.7 µF + 100 nF; the mux soft-starts the main board
supply. Check actual inrush, recovery current and USB suspend behavior on the
first assembled board. No USB compliance certification is implied.

## Placement and capacitor review — 2026-10-14

The required connectors and OLED stay fixed. U2 moved toward the first DE9
connector; U3 now occupies the MCU's right side. U8 was moved and rotated toward
the stacked USB-A connector. Pull-up arrays and local bypass parts were moved
with the signal groups. The signal-net minimum-spanning-tree estimate fell
from 1904 to 1607 mm (15.6%); this is a placement metric, not measured routed
length, timing or signal-integrity proof.

Most resistors and local bypass capacitors are already **0402**. The OLED
pull-ups and several inherited bypass capacitors use **0603**. The 10 µF supply
capacitors remain **0805**. Replacing all 0805 parts with 0603 would save little
board area and can reduce effective capacitance at operating voltage.

For C10/C11, Samsung's typical 25°C DC-bias curves give:

| Part | Package / rating | Effective pair capacitance at 3.3 V |
|---|---|---|
| Former C59461, CL10A226MQ8NRNC | 0603, 22 µF / 6.3 V | about 19.5 µF |
| Selected C45783, CL21A226MAQNNNE | 0805, 22 µF / 25 V | about 29.8 µF |

These typical values exclude initial tolerance, temperature and aging; they
are not guaranteed minima. The larger parts provide more transient margin.
Sources: [Samsung 0603 curves](https://product.samsungsem.com/mlcc/CL10A226MQ8NRN.do),
[Samsung 0805 curves](https://product.samsungsem.com/mlcc/CL21A226MAQNNN.do), and
[AP63203 application guidance](https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf).
C3's displayed voltage was also corrected to 25 V to match its existing C15850
part number.

### USB-A bulk capacitor

150 µF is a sensible nominal value: at −20% initial tolerance it gives 120 µF.
The USB hub guidance calls for at least 120 µF of low-ESR reservoir capacitance
per hub; that can be shared by these two adjacent ports on the same power rail.
See [Microchip AN17](https://www.microchip.com/content/dam/mchp/documents/OTH/ApplicationNotes/ApplicationNotes/AN17-APID.pdf).

The former RVT standard electrolytic's supplier table leaves the ESR limit
blank. C4 is upgraded to **220 µF / 10 V polymer, C19270700**, with
specified **35 mΩ at 100 kHz**, in approximately the same 6.6 mm square body
area. Its initial minimum capacitance is 176 µF. It is listed at $0.1657 each
at quantity 5 ($0.1302 at 50), before shipping/taxes, checked 2026-10-14.
[Supplier part](https://www.lcsc.com/product-detail/C19270700.html),
[manufacturer family datasheet](https://datasheet.lcsc.com/datasheet/pdf/25cd8a66a5cec751f287aca4fb3a3d5c.pdf?productCode=C19270700).

C4 remains between the keyboard and stacked USB connectors: their spacing
leaves room for it, and its supply connection can run beside the USB connector.
It does not obstruct the OLED. Rotating the original capacitor into the narrow
gap caused courtyard conflicts; the horizontal orientation is retained.
The supplier lists 5.8 mm height, while the family table shows a 7 mm variant;
confirm the exact ordered part. The case clearance model uses 8 mm, also
covering the 7.72 mm height of the supplier's STEP model.

### Two 12 MHz crystals

Y1 is the RP2350A clock reference and Y2 is the CH334P USB hub reference.
Each passive crystal belongs to a separate on-chip oscillator. Connecting one
crystal to both oscillators is not a valid replacement. A common active clock
would need a clock distribution and startup design; using an MCU-generated
clock would also couple hub operation to firmware/reset behavior.

The WCH datasheet section 6.2 describes a crystal-free mode, but says some
parameters may depart from USB requirements and availability depends on the
ordered variant. Both crystals are retained by the 2026-10-14 design decision.
WCH's v2.91 datasheet specifies approximately 16 pF internally on each XI/XO
pin and recommends no external load capacitors. Matching the selected
10 pF-load crystal still requires allowance for board and pin capacitance.
Sources: [WCH datasheet](https://www.wch.cn/downloads/CH334DS1_PDF.html)
and [RP2350 reference design](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf).
C5 provides local 3.3 V ceramic bypass near the USB service circuitry.

A lower-ESR capacitor helps hot-plug droop, but also changes charging inrush.
Measure both USB hot-plug behavior and startup through the actual Amiga cable.
The larger reservoir is behind U11, which disconnects it during recovery.

### Smaller flash option

A 4 × 3 mm USON flash, **W25Q32JVUUIQ / C2999380**, could replace the current
SOIC-8 flash without reducing the 4 MB capacity. Current LCSC listings put it
at about $2.53 versus $1.65 for the selected C5355146 at quantity 1, or $1.91
versus $1.17 at quantity 100. It saves package area but increases BOM cost and
requires a new footprint and routing. The SOIC part is retained because it fits.
Sources: [SOIC flash](https://www.lcsc.com/product-detail/C5355146.html) and
[USON flash](https://www.lcsc.com/product-detail/C2999380.html), checked 2026-10-14.

## Sourcing and footprint provenance

[BOM.csv](BOM.csv) and [SOURCING.csv](SOURCING.csv) list **110 assembled parts** (45 unique LCSC codes).
Every purchased schematic part has an `LCSC Part #` field. Holes and bare test
pads are non-BOM geometry and have no part number. The OLED module and enclosure
hardware are external assemblies, as in r6; the PCB BOM includes J3.
`assembly-accessories.json` specifies the retained r6 SH1106 module and mounting
hardware. The r6 records give its drawings and pinout, but no manufacturer
part number or LCSC code; do not invent a purchasing code or substitute a
GND-first module.

`lcsc.pretty` contains LCSC/EasyEDA imports plus supplier-drawing corrections.
Original imports are retained in `vendor-originals`. The DE9 footprint was
reconstructed from its LCSC supplier drawing because no usable imported model
was available. The local `r7.pretty` library also contains the reset supervisor
and ALPS menu-switch footprints.

Corrections are reproducible in `scripts/prepare_libraries.py`: RM2 pad geometry
and antenna keepout, connector shield pin numbers, USB-C combined power lands,
NPTH locating pegs, stacked USB shield row pitch, and four paste apertures on
the RP2350 exposed land. The MCU's local regulator/decoupling topology follows
the official [RP2350 hardware design reference](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf);
its reference PCB and license are in `reference/`.

Factory assembly was selected on 2026-10-14. The public JLC assembly stock
snapshot in [assembly-stock-check.json](assembly-stock-check.json) covers all
43 of the 45 part numbers and records stock at that check. The supervisor
and menu switch need a fresh availability check. Direct retail stock is recorded
separately in [stock-check.json](stock-check.json). Retail unavailability is
not the same as unavailability in the assembler's inventory. These snapshots
are dated evidence, not reserved material or an accepted assembly quote.

| Item | Selected LCSC part | JLC public stock on 2026-10-14 |
|---|---|---:|
| Stacked USB-A | C2341 | 3,310 |
| Mini-DIN | C77848 | 2,060 |
| Two female DE9 connectors | C50380980 | 1,711 |
| RM2 radio | C49451547 | 117 |
| RP2350 regulator inductor | C42411119 | 75 |
| Recovery latch | C2866652 | 619 |

The r6 HOOYA DIN-603 / C7428703 really was stocked in the earlier snapshot.
It now has zero direct LCSC stock and only one in public JLC stock, below the
listed placement minimum. J7 uses **CONNFLY DS1093-01-PN60 / C77848** instead.
Its drawing specifies 1.1 mm signal holes, 2.3 mm shield holes and 13.7 mm shield
spacing. The front face stays at the existing case opening. The supplier
footprint needed corrections to shield-hole size and central-tab position;
these are recorded in `scripts/adopt_connfly_din.py`. C54573054 is a larger
full-size DIN connector, so it is not a substitute for this mini-DIN socket.

Keep the exact Abracon L1: winding polarity is part of the RP2350 regulator
reference layout. The factory's small stock count needs reconfirmation before
ordering. Do not substitute an arbitrary 3.3 µH inductor.

R9's LCSC number was corrected from C25118 (47 Ω) to **C25117 (470 Ω)** to
match the RM2 DOUT-combining circuit. The other selected resistor/capacitor
values were compared with supplier descriptions. D4–D6 add ground-referenced
TPD2EUSB30 ESD protection at both USB-A channels and the USB-C service channel;
these add about $2.05 in initial-tier parts cost. U2/U3 remain cost-review
candidates, but a substitute must retain 5 V signal handling, 3.3 V translation
and specified power-off isolation.

### Stacked USB-A availability and fit

The selected J4 **907-111A1012D10200 / C2341** is available: the direct LCSC
page checked on 2026-10-14 lists **3,310 immediately shippable units**, minimum
and order multiple five, at **$0.1855 each for five** before discounts,
shipping and taxes. The earlier out-of-stock sourcing note has been corrected.
[LCSC listing](https://www.lcsc.com/product-detail/C2341.html).

The board has the drawing's 13.10 mm shield-column pitch, 5.80 mm shield-row
pitch and 2.60 mm contact-row pitch. Shield holes are 2.30 mm; all eight contact holes now use the drawing's
0.92 mm recommendation. The 14.60 mm body width excludes the wider 15.50 +/- 0.20 mm
features. The case opening is now 16.90 mm wide, allowing 0.60 mm per side around
the 15.70 mm maximum envelope. Check retention on the 1 mm PCB too.
[Supplier drawing](https://datasheet.lcsc.com/datasheet/pdf/a0b416dc2d0ad40e281371fbd00e5e99.pdf?productCode=C2341).

The supplier drawing calls the housing high-temperature thermoplastic, while
the LCSC Chinese product description says it is not high-temperature rated.
Neither establishes a soldering temperature/time profile. Plan separate
through-hole assembly after SMT reflow unless the supplier confirms a suitable
profile. [LCSC Chinese listing](https://item.szlcsc.com/2717.html).

## Review results and remaining order gates

The final configured KiCad checks report **zero ERC violations, zero DRC
violations, zero unconnected items and zero schematic parity issues**.
`scripts/audit_board.py` additionally checks all 110 purchased parts are on top,
all have project-relative models, the 70 × 66 mm outline, connector drills,
absence of bottom silkscreen and absence of L3 signal crossings beneath the
bottom USB pairs. USB layer transitions have ground holes within 2 mm.
L2 contains no signal tracks. These checks are not measured signal integrity.

The explicit **JLC04101H-7628 nominal 1 mm** stackup is recorded in the board.
Main outer USB pairs use 0.2474 mm width / 0.1501 mm gap for the calculator's
90 Ω target; short pad escapes and the USB2 connector neck use narrower copper.
The fabricator must confirm the construction and impedance. The filled L3
plane was inspected after moving the control routes out of the USB corridor.

Both case halves rebuild as closed, single-solid meshes. The modeled hardware
intersection is empty, including the new USB shield-tail floor pockets. This
is a nominal geometry check; connector retention, tolerance and actual fit
still need the first assembled board. The retained r6 OLED drawings are in
`../case_r7/reference/`, with the same 3V3–GND–SCL–SDA pin order.

**Specify epoxy-filled, copper-capped electrical vias (IPC-4761 Type VII).**
The layout has thermal and other vias in or close to solder lands. Tenting
alone does not prevent solder loss there. Connector and mounting holes must
remain open. The native PCB and exported via list identify the holes to fill.
This can increase four-layer fabrication cost; obtain a quote for this exact
process, nominal 1 mm thickness and ENIG. See [JLC via covering guidance](https://jlcpcb.com/help/article/pcb-via-covering)
and [manufacturing-requirements.json](manufacturing-requirements.json).

Before ordering, obtain the factory's acceptance of:

1. The stackup, filled/capped via process, impedance target and thickness.
2. All 45 part numbers, actual batch allocation, placement minima and attrition.
3. Factory through-hole soldering after SMT reflow, 1 mm connector retention,
   and a placement preview confirming the explicit JLC rotations and polarity.
4. Installation of the retained r6 OLED and specified nylon hardware.

The local `production/factory-review-*.zip` package contains Gerbers, separate
plated/nonplated drills, IPC-D-356 netlist, BOM, corrected JLC CPL, via list,
editable KiCad sources, schematic/layer PDFs, board STEP and case STL files.
It is marked for quotation/CAM review, pending manufacturing acceptance.

### Assembly export defaults

Identical purchased parts now have identical Value, Footprint and LCSC fields.
Circuit roles such as CC1, BOOTSEL and hub decoupling are in the separate
`Function` field. Fabrication Toolkit therefore emits **45 BOM rows for 110
components**, without splitting identical LCSC parts across multiple rows.

The project saves **Apply automatic translations = off** in
`fabrication-toolkit-options.json`. Each assembled symbol and PCB footprint has
an explicit `FT Rotation Offset` from `assembly-orientation.json`. Do not turn
generic translations back on: its package-name rules rotate these LCSC parts
incorrectly. This is the documented [Fabrication Toolkit field mechanism](https://github.com/bennymeg/Fabrication-Toolkit#attributes).

The audit corrected the exported rotations of U1, U2, U3, U5, U8, U9, U11,
U12 and Q1-Q3, plus L1's dotted end and D3's cathode. D2 is correct with its
cathode toward the bottom edge. The L1 and D3 STEP orientations also now match
the physical pins. Twenty-five footprints have physical pin-1 dots, cathode
bars or a positive-terminal mark on top silkscreen; the numbered-pad drawing
is `production/assembly-orientation.svg`.

Regenerate manufacturing files from the checked board before ordering.
Existing stock/rotation snapshots do not cover the supervisor and menu switch.
Check U13 pin 1 and SW2 contact orientation in the factory placement preview.

For a new upload use `production/gerbers.zip`, `production/bom.csv` and
`production/positions.csv`, generated together after the checks below.
The current browser order retains its earlier upload and manual corrections;
local exports do not change that order. RM2 and DE9 lack useful JLC placement
models, so factory CAM must use the supplier drawings and numbered pads.

[Electrical review and prototype test plan](electrical-review.json) records
startup, recovery, source takeover, backfeed, inrush and voltage-budget review.
In particular, the shared PTC is not a precise 500 mA current limiter and does
not guarantee 500 mA at either USB port. At 0.5 A its specified 0.70 Ω maximum
post-trip resistance alone would lose 0.35 V; switches, cable and copper add
further loss. Establish the intended load from measurements.

The first order should be a small prototype batch. Build the
[`BOARD_HIDPICO_REV7` firmware](../doc/installation.md#rev7-rp2350a-and-rm2), then test
USB-C-only recovery with invalid flash, two peripherals, Amiga supply
current/droop/backfeed, radio operation and case fit. The firmware has not
been physically validated on r7.

### Cost discussion

The public assembly snapshot gives **$23.7146 per board** at initial part-price
tiers. This excludes OLED, mounting hardware, PCB, assembly/setup, case,
shipping and tax; it is not a factory quote. The RM2 is about $6.53, the two
interface buffers total $4.07, and the three USB protectors total $2.05.
The buffers and protection parts are useful future cost-review candidates,
provided replacements preserve voltage tolerance, power-off isolation and ESD
performance. Both independent crystals and the current SOIC flash are retained.
Factory setup and filled-via charges may matter more for a small batch than
minor resistor/capacitor savings.

## Rebuilding and validation boundaries

`circuit.json` is the source for `scripts/build_schematic.py`, which regenerates
the schematic and BOM. `scripts/build_board.py` creates a **fresh placement and
overwrites routing**. `scripts/prepare_routing.py` adds reviewed local routing
once to that fresh board. Do not rerun these board builders on hand-edited
routing without retaining a backup. `scripts/configure_rules.py` sets the board
constraints and power net classes.

Run `scripts/audit_board.py`, `scripts/check_reset.py` and
`scripts/check_menu.py` with pcbnew Python
after filling zones. The reset check verifies the supervisor pinout and keeps
automatic RUN resets separate from the manual recovery request. Run KiCad
ERC and DRC with schematic parity, then `scripts/export_factory_review.py`
with pcbnew Python to
export the quotation package; it refuses stale checks or reported violations.
The current outputs live in `review/` and `production/`. Refresh these after
any change. The JSON ERC/DRC reports list configured ignored checks, including
library footprint filters, missing courtyards and tuning-profile geometry;
there are no per-item DRC exclusions. Copper clearances and connectivity remain
enabled. DRC does not prove electrical correctness or physical fit.

USB recovery current/suspend behavior, clock startup, RF behavior, two-device
operation, Amiga backfeed isolation and connector insertion forces require the
hardware tests in `electrical-review.json`.

### Cold-start reset verification

The supervisor threshold and delay follow the
[TI TPS3808 datasheet](https://www.ti.com/lit/ds/symlink/tps3808.pdf).
The selected W25Q32JV requires 20 µs from VCC(min) to chip select going low.
Check +3V3 at the flash, RUN, RECOVERY_N and QSPI-CS on an oscilloscope:
normal keyboard-powered and USB-C-powered starts must boot application flash;
held RECOVERY with USB-C must enter ROM recovery. Repeat with slow supply ramps,
short power interruptions and source handover. Supervisor addition has not
been physically validated.

### Menu lever wiring and assembly

SW2 is an [ALPS SLLB510100](https://tech.alpsalpine.com/e/products/detail/SLLB510100/)
on the USB-C edge, aligned with the case slot at X=35 mm. Terminal C and the
mechanical anchors connect to ground. The three contacts are active low:

| Contact | Net | RP2350 GPIO | Pull-up |
|---|---|---|---|
| 1 / CCW | MENU_LEFT_N | 14 | R27, 10 kΩ to 3.3 V |
| 2 / CW | MENU_RIGHT_N | 15 | R28, 10 kΩ to 3.3 V |
| T / push | MENU_PUSH_N | 16 | R29, 10 kΩ to 3.3 V |

The REV7 firmware debounces these contacts and provides
[lever menu controls](../doc/runtime-configuration.md#rev7-menu-lever). RECOVERY remains a
separate hardware function. With the board powered, check each input is high
at rest and goes low only for its corresponding action before enabling menu
operation. Confirm the desired left/right sense with the assembled enclosure.

The switch footprint/model originated in the
[JLCEDA](https://lceda.cn/)/[EasyEDA Official Library](https://easyeda.com/),
part C125039. The four signal/common lands use the ALPS 1.0 × 1.6 mm dimensions;
the two 1.1 mm locator holes are unplated. The courtyard reserves the case's
conservative 14.2 × 9.5 mm motion envelope with 0.1 mm assembly clearance.
Check the supplier drawing and a physical sample before production.
The project-relative WRL model has a -0.2 mm Z correction to put the body top
at 2.2 mm and the locating lugs 0.6 mm below the seating plane. The importer's
untransformed STEP is deliberately not used; check exported factory 3D files
for the switch rather than assuming WRL inclusion in a STEP export.

The power components leave this movement envelope clear. Verify regulator
startup, ripple and load transients on the revised placement. Procurement
stock and tape orientation for C125039 still require factory confirmation.
