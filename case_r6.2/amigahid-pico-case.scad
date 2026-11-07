// AmigaHID r6 enclosure. Derived from ../amigahid-pico-case; YAPP MIT library.
// World X = KiCad X, world Y = 68 - KiCad Y. All dimensions in mm.
include <pcb-mechanics.scad>

/* [Output] */
view = "print"; // [print,base,lid,assembled,fit,collision]
oled = true;
external_power_opening = false;
a500_cable_opening = false;
/* [Amiga styling] */
amiga_style = true;
edge_bevel = 2.6; // sloping shoulder around the lid
oled_window_bevel = 0.6; // 45-degree exterior rim; zero keeps a square window
rib_pitch = 4.6;
rib_width = 1.3; // recessed gaps leave broad, flat ribs
rib_depth = 0.7; // blind grooves, not ventilation holes
end_rib_pitch = 3.6;
end_rib_width = 2.0; // gap width; leaves 1.6 mm ribs
end_rib_recess = 0.5; // entire ribbed band sits below the surrounding shell
end_rib_depth = 0.5; // additional groove depth below the recessed rib tops
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
if (amiga_style) {
    assert(edge_bevel >= 0 && edge_bevel <= 2.6);
    assert(oled_window_bevel >= 0 && oled_window_bevel <= 1.0);
    assert(rib_pitch > rib_width && rib_width > 0);
    assert(rib_depth > 0 && rib_depth <= 0.7);
    assert(end_rib_pitch > end_rib_width && end_rib_width > 0);
    assert(end_rib_recess > 0 && end_rib_depth > 0);
    assert(end_rib_recess + end_rib_depth <= 1.0);
}

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

// A blind recess following the top, bevel and -Y wall. Depth is measured
// vertically on the roof and horizontally on the wall, as in the reference.
module end_band_cut(x, width, depth) {
    translate([x,-0.1,case_z-depth])
        cube([width,8.1,depth+0.1]);
    translate([x,-0.1,case_z-6])
        cube([width,depth+0.1,6.1]);
    if (edge_bevel > 0)
        translate([x,0,case_z-edge_bevel])
            rotate([0,90,0]) linear_extrude(width)
                polygon([[depth+0.1,-0.1],
                         [-edge_bevel-0.1,-0.1],
                         [-edge_bevel-0.1,edge_bevel+0.1],
                         [-edge_bevel+depth-0.1,edge_bevel+0.1]]);
}

// Exterior relief inspired by the broad top ribs and comb-like end of case.stl.
// Work in assembled coordinates; leave the roof, ports and blind snaps intact.
module lid_style_cuts() {
    // Enlarge only the exterior mouth of the OLED opening. The lower profile
    // matches the existing 32.22 x 17.5 mm cutout with 0.5 mm corner radii.
    if (oled && oled_window_bevel > 0)
        translate([ox+window_xy[0],oy+window_xy[1],case_z]) hull() {
            translate([0,0,-oled_window_bevel]) linear_extrude(0.01)
                offset(r=0.5) square([32.22-1,17.5-1],center=true);
            linear_extrude(0.1)
                offset(r=0.5+oled_window_bevel)
                    square([32.22-1,17.5-1],center=true);
        }
    // Trim only the outside shoulder, keeping the original inner shell.
    if (edge_bevel > 0) difference() {
        translate([-1,-1,case_z-edge_bevel])
            cube([case_x+2,case_y+2,edge_bevel+1]);
        hull() {
            translate([0,0,case_z-edge_bevel-0.01])
                cube([case_x,case_y,0.01]);
            translate([edge_bevel,edge_bevel,case_z])
                cube([case_x-2*edge_bevel,case_y-2*edge_bevel,1]);
        }
    }
    // Six transverse gaps on the rear half, clear of the display surround.
    for (i=[0:5])
        translate([-0.1,case_y-13-i*rib_pitch-rib_width/2,case_z-rib_depth])
            cube([case_x+0.2,rib_width,rib_depth+0.1]);
    // Recess the complete front band first, including its rib tops. The
    // reference has 2 mm gaps, ~1.6 mm ribs and two distinct 0.5 mm steps.
    end_band_cut(3,case_x-6,end_rib_recess);
    end_count = floor((case_x-12)/end_rib_pitch);
    for (i=[0:end_count]) {
        x = case_x/2 + (i-end_count/2)*end_rib_pitch;
        end_band_cut(x-end_rib_width/2,end_rib_width,
                     end_rib_recess+end_rib_depth);
    }
}

module case_shell() {
    difference() {
        YAPPgenerate();
        if (amiga_style && printLidShell) color(colorLid) {
            if ($preview && !showSideBySide) lid_style_cuts();
            else
                // Match YAPP's face-down, mirrored lid on the print plate.
                translate([0,2*case_y+10+shiftLid,case_z])
                    rotate([180,0,0]) lid_style_cuts();
        }
    }
}

if(view=="collision") intersection() { let($preview=true) case_shell(); hardware_envelopes(); }
else {
    if(view=="assembled") let($preview=true) case_shell();
    else if(view=="lid") translate([0,-(case_y+10+shiftLid),0]) case_shell();
    else case_shell();
    if(view=="fit") hardware_envelopes();
}
echo("r6 case outside XYZ",[case_x,case_y,case_z]);
echo("r6 PCB top Z / OLED glass top Z",board_top,board_top+oled_standoff+oled_board_thickness+oled_glass_height);
