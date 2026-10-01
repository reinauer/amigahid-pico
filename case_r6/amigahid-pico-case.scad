// AmigaHID r6 enclosure. Derived from ../amigahid-pico-case; YAPP MIT library.
// World X = KiCad X, world Y = 68 - KiCad Y. All dimensions in mm.
include <pcb-mechanics.scad>

/* [Output] */
view = "print"; // [print,base,lid,assembled,fit,collision]
oled = true;
external_power_opening = false;
a500_cable_opening = false;
/* [Fit adjustments] */
pcb_clearance = 0.6;
port_clearance = 0.6; // per side, beyond supplier nominal dimensions
pilot_diameter = 1.7; // M2 tapped pilot; use M2x4 nylon screws
// Board top to underside of the OLED PCB, using H1-H4 nylon spacers.
oled_standoff = 12;
oled_board_thickness = 1.2;
oled_glass_height = 2.6;
oled_window_y_trim = 0;
// Inputs used by YAPP overrides must exist before its include.
ox = 2.2 + pcb_clearance;
oy = 2.2 + 3;
board_top = 3 + 3 + board_t;
ceiling = 3 + 6 + 15;
case_x = board_x + 2*pcb_clearance + 2*2.2;
case_y = board_y + 3 + pcb_clearance + 2*2.2;
case_z = ceiling + 2.5;
oled_corner = [h1[0]-2.5, h1[1]+2.55];
window_xy = [oled_corner[0]+17.75, oled_corner[1]-17.4+oled_window_y_trim];
include <vendor/YAPP_Box/YAPPgenerator_v3.scad>

/* [Hidden] */
pcbLength = board_x;
pcbWidth = board_y;
pcbThickness = board_t;
standoffHeight = 3;
standoffDiameter = 4.4;
standoffPinDiameter = 2;
standoffHoleSlack = 0.2;
paddingFront = pcb_clearance;
paddingBack = pcb_clearance;
paddingLeft = 3; // air gap beyond the antenna end
paddingRight = pcb_clearance;
wallThickness = 2.2;
basePlaneThickness = 3;
lidPlaneThickness = 2.5;
baseWallHeight = 6; // low split lets connectors drop into the open base
lidWallHeight = 15;
ridgeHeight = 4.5;
ridgeSlack = 0.25;
ridgeGap = 0.25;
yappSnapOuterSkin = 0.8; // blind snap pockets: continuous exterior wall
yappSnapRadialSlack = 0.15; // clearance between pocket and shortened base bump
roundRadius = 2;
boxType = 3;
renderQuality = 12;
previewQuality = 8;
printMessages = false;
showOrientation = false;
showPCB = false;
showSideBySide = view == "print";
onLidGap = 0;
printBaseShell = view != "lid";
printLidShell = view != "base" && view != "fit";
printSwitchExtenders = false;
printDisplayClips = false;
colorBase = "SlateGray";
colorLid = "Gainsboro";
alphaBase = 1;
alphaLid = 1;

assert(ceiling - (board_top+oled_standoff+oled_board_thickness+oled_glass_height) >= 0.4,
       "OLED glass needs at least 0.4 mm clearance under lid");
assert(port_clearance >= 0.3);

// Connector cutouts use PCB coordinates, with Z measured above the main PCB.
// +X wall: mini-DIN opening accepts a <=14.4 mm plug moulding; USB uses full shell.
cutoutsFront = [
    [j7[1], 6.55, 0, 0, 7.5, yappCircle, 0, 0, yappCenter, yappCoordPCB],
    [usb_y, 3.545, 14.5+2*port_clearance, 7.09+2*port_clearance,
     0.5, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
];
// +Y wall: full DE9 flange/body relief permits top-down PCB installation.
// Centres come from the two shell mounting pads, NOT the footprint origin.
cutoutsRight = [for (x=de9_centres)
    [x, 6.25, 30.8+2*port_clearance, 12.5+2*port_clearance,
     0.4, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
];
// Optional external 5V pigtail: J9 remains a header, not a panel power jack.
cutoutsLeft = external_power_opening ? [
    [j9[0]+1.27, 5, 9, 7, 1.5, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
] : [];
cutoutsBack = a500_cable_opening ? [
    [j1[1]-8.89, 5, 6, 7, 1.5, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
] : [];
cutoutsLid = oled ? [
    [window_xy[0], window_xy[1], 32.22, 17.5, 0.5,
     yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
] : [];
// Snap locations avoid the connector apertures and the antenna end.
snapJoins = [[20+oy,6,yappBack], [55+oy,6,yappBack],
             [30+ox,6,yappLeft], [44+ox,6,yappLeft]];
pcbStands = [];
connectors = [];

// Only H5/H6 support the main board. H1-H4 carry the OLED on separate spacers.
module hookBaseInside() {
    for (h=[h5,h6]) translate([paddingBack+h[0],paddingLeft+h[1],-0.01])
        difference() {
            cylinder(d=4.4,h=standoffHeight+0.01,$fn=48);
            translate([0,0,-0.02]) cylinder(d=pilot_diameter,h=standoffHeight+0.06,$fn=36);
        }
}

// Inspection-only envelopes. Never included in printable base/lid exports.
module pcb_envelope() {
    translate([ox,oy,basePlaneThickness+standoffHeight+0.01]) difference() {
        cube([board_x,board_y,board_t-0.01]);
        translate([10,-0.01,-0.01]) cube([14,9.01,board_t+0.02]);
        for(h=[h1,h2,h3,h4,h5,h6]) translate([h[0],h[1],-0.01])
            cylinder(d=(h==h5||h==h6)?2.2:3.2,h=board_t+0.02,$fn=32);
    }
}
module hardware_envelopes() {
    color("ForestGreen") pcb_envelope();
    // Supplier full-body envelopes, conservative at connector corners.
    for(x=de9_centres) color("Silver")
        translate([ox+x-15.4,oy+board_y-9.8,board_top]) cube([30.8,18.91,12.5]);
    color("Silver") translate([ox+57.28,oy+j7[1]-7.1,board_top]) cube([12.8,14.2,13.1]);
    color("Silver") translate([ox+55.77,oy+usb_y-7.25,board_top]) cube([15.22,14.5,7.09]);
    color("SeaGreen") translate([ox+u1[0]-10.5,oy+u1[1]-25.5,board_top+0.1]) cube([21,51,4.0]);
    if(oled) {
        color("DarkGreen") translate([ox+oled_corner[0],oy+oled_corner[1]-33.7,board_top+oled_standoff]) cube([35.5,33.7,oled_board_thickness]);
        color("MidnightBlue") translate([ox+window_xy[0]-17.25,oy+window_xy[1]-11.5,board_top+oled_standoff+oled_board_thickness]) cube([34.5,23,oled_glass_height]);
    }
}

if(view=="collision") intersection() { let($preview=true) YAPPgenerate(); hardware_envelopes(); }
else {
    if(view=="assembled") let($preview=true) YAPPgenerate();
    else if(view=="lid") translate([0,-(case_y+10+shiftLid),0]) YAPPgenerate();
    else YAPPgenerate();
    if(view=="fit") hardware_envelopes();
}
echo("r6 case outside XYZ",[case_x,case_y,case_z]);
echo("r6 PCB top Z / OLED glass top Z",board_top,board_top+oled_standoff+oled_board_thickness+oled_glass_height);
