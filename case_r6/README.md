# AmigaHID Pico r6 case

YAPP enclosure for `../kicad_r6/amigahid-pico.kicad_pcb`. Outside dimensions are
**75.6 × 76.0 × 26.5 mm**. The base and lid are separate snap-fit parts. The lid uses **blind internal clip
recesses**, leaving 0.8 mm of continuous exterior wall. Matching base clips are
shortened with 0.15 mm clearance; print the updated base and lid together.

Run the build command below to generate the STL files and previews locally.
Generated `build/` files are intentionally excluded from Git.

## Print files

- [Base STL](build/base.stl), floor down.
- [Lid STL](build/lid.stl), exterior top face down; already oriented for printing.
- [Both parts](build/print.stl), separated on one build plate.
- [Editable OpenSCAD design](amigahid-pico-case.scad).
- [PCB and connector fit preview](build/fit.png), with simplified component envelopes.

PETG and a 0.2 mm layer height are reasonable starting settings, carried over from
the original case. Tune `ridgeSlack` and the port clearance after a first print.
This is a geometrically checked prototype, **not a physically test-fitted enclosure**.

## r6 changes

The original sibling repository was copied into this directory. Its original SCAD, README, assets and unused STEP helpers remain local-only
and are excluded from Git; the source license is retained. The r6 SCAD uses the actual
[YAPP_Box generator](https://github.com/mrWheel/YAPP_Box), vendored at commit
`f9400c419ef1dea7dc0b3607876989b4f3faa2b7` with its MIT license. Original case source:
`../amigahid-pico-case`, commit `84d0b1838114eac426ddd242eb8ad227578d349e`.

Coordinates are extracted from the current PCB, not copied from the old case.
Case X = KiCad X; case Y = 68 − KiCad Y, plus the wall and padding offsets.
The PCB origin lies at case (2.8, 5.2) mm, with the PCB top at Z=7.6 mm.

| Feature | Position and opening |
|---|---|
| J5/J6 DE9 | Two 32.0 × 13.7 mm openings on the +Y wall, at PCB X=17.54 and 51.54 mm; includes flange and screw access. |
| J7 A4000 mini-DIN | Ø15 mm opening on the +X wall, at PCB Y=20 mm, centre 6.55 mm above PCB. |
| J4 USB-A | 15.7 × 8.29 mm opening on the +X wall, at PCB Y=50.5 mm, centre 3.545 mm above PCB. |
| OLED | 32.22 × 17.5 mm lid window centred at PCB (51.0, 48.2) mm; matches the supplied 1.3-inch SH1106 drawing. |
| H5/H6 | Ø4.4 mm printed supports, 3 mm above the floor, at PCB (2.7,63) and (53.3,22.5) mm. |
| J9 external 5V | Optional 9 × 7 mm pigtail opening on the −Y wall, aligned with J9. Disabled by default. |
| J1 A500 cable | Optional 6 × 7 mm opening on the −X wall. Disabled by default for mini-DIN keyboard use. |

Connector openings use supplier mechanical drawings, including the offset between
DE9 footprint origins and their actual shell centres. Nominal shell dimensions:
DE9 30.8 mm wide × 12.5 mm high; JST USB-A 14.5 × 7.09 mm; DIN-603 14.2 mm
wide × 13.1 mm high. Supplier drawing URLs are recorded in `../kicad_r6/SOURCING.csv`.
The DIN opening allows plug mouldings up to about 14.4 mm diameter with clearance;
measure unusually bulky cable boots before printing.

The Pico antenna has the PCB's 14 × 9 mm notch beneath it and an extra 3 mm gap
at that end of the enclosure. There are no support posts in that region. Use a
nonmetallic case and nylon hardware; the base remains a continuous plastic floor.

## Assembly

1. Clear/tap the two 1.7 mm support pilots for M2 threads. Fix the PCB with
   **two M2 × 4 mm nylon screws** through H5/H6. Longer screws can bottom out.
2. Mount the OLED on **four 12 mm M3 nylon spacers** through H1–H4. The case
   supports the main board; it does not duplicate the OLED mounts in the lid.
3. The display stack assumes a 1.2 mm module PCB and 2.6 mm glass height above
   it. This leaves **0.6 mm** beneath the lid. Confirm the assembled stack before
   snapping shut; adjust `oled_standoff`, the stack dimensions and case height
   together if your module differs. The female J3 socket needs sufficient mating
   pin length to bridge the 12 mm spacing; use flying leads if necessary.
4. Set JP1 to the intended power source, connect the display and close the lid.
   Pico programming USB, BOOTSEL, JP1 and debug/experimental headers are accessed
   with the lid removed. No programming extension or push-button is included.

J9 is a PCB header, **not a panel power jack**. Its opening is for a low-profile
wired pigtail with strain relief added during assembly. A tall straight Dupont
plug under the OLED will not fit this stack. The wall is closed by default for Amiga keyboard power. Set
`external_power_opening=true` to enable the auxiliary-power cable opening.
The optional J1 cable opening similarly assumes flexible/low-profile wiring.

## Rebuild and checks

Open `amigahid-pico-case.scad` in OpenSCAD. `view` supports `print`, `base`, `lid`,
`assembled`, `fit` and `collision`. Change `oled`, `external_power_opening` or
`a500_cable_opening` to select apertures.

```sh
# From the repository root; requires KiCad's Python module.
/usr/bin/python3 case_r6/tools/extract_pcb.py
# Requires OpenSCAD on PATH, or OPENSCAD=/path/to/openscad.
python3 case_r6/tools/build.py
```

The generated, local-only `pcb-mechanics.json` records the source PCB hash and measured component positions.
`pcb-mechanics.scad` is a small committed geometry input so OpenSCAD can open
the design without a KiCad Python installation.
Extraction updates placement coordinates; board outline and connector type changes
still need mechanical review. Current cutout dimensions and pin-centre corrections
are for the selected r6 component part numbers.

The exported base and lid were checked for closed edges, positive volume, and
exactly one connected solid each. See `build/mesh-validation.json`. The modelled
PCB, connector, Pico and OLED envelopes have **zero volume of interference** with
the assembled enclosure. A 0.01 mm separation excludes intentional support/PCB
contact from that Boolean test. This checks nominal body clearance; it does not
validate every cable boot, solder joint, screw, printer tolerance or snap force.

The build also exports CSG for both parts. STEP conversion is not included or
validated in this revision.
