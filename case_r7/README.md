# AmigaHID r7 case

Prototype enclosure for the **70 × 66 × 1 mm r7 PCB**. Outside dimensions are
**78 × 71.6 × 26.5 mm**, excluding projecting connector shells. All PCB
components are fitted on top. The OLED is centered on the board, beside the
stacked USB-A connector. RM2 faces the left wall; USB-C and the recessed
RECOVERY actuator are on the bottom edge near the right corner.

The PCB extends 6 mm under the DE9 housings. KiCad coordinates retain their
original origin: the board spans Y=-6..60 mm. Connector and OLED positions
are unchanged. The flange is 0.52 mm beyond the extended PCB edge and its
nose projects 6.42 mm. Full-body openings provide clearance through the wall;
check the intended cable hoods and retaining screws on the prototype.

## Shared OLED and PCB mounts

H1–H4 now secure the main PCB to the base as well as carrying the OLED.
**H5/H6 and their separate M2 fixings are removed.** Four printed Ø6.4 mm
bosses support the PCB 3 mm above the floor, on a 30.5 × 28.6 mm pattern.
Short supports beneath the edges near the connectors limit downward PCB flex
without additional holes. These are not a substitute for an insertion-force test.

The stack, from bottom to top, is:

1. Four **M3 × 12 mm countersunk nylon screws**, inserted from underneath.
2. A 3 mm base floor, 3 mm printed support, and 1 mm main PCB.
3. Four **12 mm M3 female/female nylon spacers** above the PCB. Hold each spacer
   while tightening its bottom screw; approximately 5 mm of thread engages it.
4. OLED module, assumed 1.2 mm thick, secured with four **M3 × 5 mm nylon screws**.

The through-bores are 3.4 mm; the 90-degree countersinks are 6.4 mm at the mouth.
Measure the selected screw heads and spacer thread depths before assembly.
Do not countersink the PCB or tighten against the OLED glass. The base screws
clamp the main PCB independently of the OLED. If the OLED is omitted, retain
spacers or suitable nylon nuts to secure the main PCB.

The display glass is modeled 2.6 mm above its module PCB, leaving **1.2 mm**
beneath the lid. J3 needs a mating connection suitable for the 12 mm gap; verify
header engagement or use short insulated leads. Keep solder tails within the
3 mm clearance beneath the main PCB.

## Ports and styling

| Feature | Geometry / location in PCB coordinates |
|---|---|
| DE9 ports | Centers X=17.54 and 51.54 mm; openings 32 × 13.7 mm |
| Mini-DIN keyboard | Right edge, Y=16.35 mm; Ø15 mm opening |
| Stacked USB-A | Right edge, Y=41.5 mm; 16.9 × 16.9 mm opening |
| USB-C | Bottom edge, X=60 mm; 10.14 × 4.46 mm opening |
| RECOVERY | Bottom edge, X=49 mm; Ø1.6 mm pinhole |
| Menu lever | USB-C edge, X=35 mm; 15.4 × 3.4 mm slot, centered 1.1 mm above the PCB |
| OLED mounts | (19.75,17.3), (50.25,17.3), (19.75,45.9), (50.25,45.9) mm |
| OLED window | 32.22 × 17.5 mm, centered at (35,32.15) mm |

Use a blunt nonconductive probe for RECOVERY. Its actuator is recessed behind
the wall; there is no loose printed plunger to lose. USB-C supplies recovery
power with the Amiga off. Connecting USB-C while the Amiga is already on
requires pressing/releasing RECOVERY. After flashing, unplug USB-C and press
RECOVERY again if Amiga power remained on.

The Amiga-style sloping shoulder, broad roof ribs, recessed front band and
0.6 mm OLED-window bevel are retained. The ribs stop around the relocated
window to leave its border intact. The reference styling was based on the
[RGBtoHDMI Amiga 500 housing](https://www.printables.com/model/63688-rgbtohdmi-adapter-housing-for-amiga-500).
Blind internal snap recesses retain a continuous 0.8 mm exterior wall.
The base tabs leave 0.05 mm nominal clearance to the backs of those recesses.
Adjust `snap_clearance` for your printer/material: increase it for easier
opening, or reduce it for more engagement. Check snap force on a printed pair.
Use the matching new base and lid together.

## ALPS menu lever

The case fits the modeled position of the
[ALPS SLLB510100](https://tech.alpsalpine.com/e/products/detail/SLLB510100/)
on the USB-C/RECOVERY edge, left of RECOVERY. Its lever rocks left/right and
pushes inward through the wall. A shallow 18 × 6.4 mm finger recess surrounds
the slot. The top and the DE9 wall remain closed apart from their existing
openings. A 10 mm wide support beneath the PCB limits downward flex at the
switch; the nearby snap is moved beside RECOVERY and shortened to 3 mm.

The intended placement has its neutral actuator tip 2 mm beyond the PCB edge
(KiCad Y=62), recessed 0.8 mm behind the case exterior. The body extends inward
to approximately KiCad Y=53.2. The clearance envelope includes the specified
0.7 mm push travel. Its 14.2 mm lateral envelope is a conservative estimate
from the supplier drawing, not a dimensioned full-motion supplier model.
Check rocking travel, finger access and soldered height with a sample before
ordering PCBs or printing a final enclosure.

SW2 on the r7 PCB occupies this position. The power circuitry sits around its
movement envelope. GPIO14/15/16 connect the CCW, CW and push contacts with
10 kΩ pull-ups; menu handling and debounce still require firmware support.
The case collision check verifies the modeled switch against the shell, not
against every PCB component. PCB courtyard checks cover the reserved movement
envelope; a soldered sample remains necessary to confirm actual fit.

Set `menu_lever=false` in OpenSCAD for a closed wall and the original snap
arrangement. Extraction reads SW2 from the PCB and sets the slot and support
position together. `menu_x` allows adjustment within X=30..35 mm, but any
manual override must match the PCB. `view="menu"` shows the assembled service edge
with an approximate switch model. The switch solders to the PCB; there is no
loose printed cap or plunger.

## Files and rebuilding

- [Base STL](build/base.stl), floor down.
- [Lid STL](build/lid.stl), already oriented exterior down.
- [Both parts](build/print.stl).
- [Shared-mount preview](build/mounts.png).
- [Hardware fit preview](build/fit.png).
- [Assembled case](build/assembled.png).
- [Menu lever preview](build/menu.png).
- [Editable source](amigahid-pico-case.scad).

```sh
# From the repository root, with pcbnew available:
/Users/stepan/git/KiCAD-MCP-Server/venv/bin/python case_r7/tools/extract_pcb.py
OPENSCAD=/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD python3 case_r7/tools/build.py
```

Extraction reads `kicad_r7` and checks the board dimensions and shared mounting
pattern. `pcb-mechanics.scad` is the generated geometry input; the local JSON
also records the PCB hash. Connector envelopes must be reviewed when parts or
orientations change. The previous case source was saved locally under
`reference/before-shared-oled-mounts-20261014/`.

The build checks closed manifold meshes, positive volume, one connected solid
per part, and zero intersection with simplified hardware envelopes. It checks
nominal geometry, **not physical fit, PCB stiffness, snap force, cable boots,
RF performance, solder tails or all individual components**. Print and test a
prototype before treating the enclosure as final. The PCB remains a routing
prototype, so case geometry may still need adjustment.

YAPP_Box is vendored at `f9400c419ef1dea7dc0b3607876989b4f3faa2b7` with its MIT
license. The original case source came from `../amigahid-pico-case`, commit
`84d0b1838114eac426ddd242eb8ad227578d349e`.

The OLED remains the r6 SH1106 module. Its original drawings are preserved in
`reference/`; assembly accessories are listed in
`../kicad_r7/assembly-accessories.json`. Four 0.8 mm deep, 3.5 mm diameter
blind pockets beneath the stacked USB shield tabs accommodate the drawing's
4.3 mm worst-case tail length below the seating plane. The 3 mm floor retains
2.2 mm beneath those pockets; the board and display height are unchanged.
