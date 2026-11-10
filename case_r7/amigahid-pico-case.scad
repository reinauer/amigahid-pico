// AmigaHID r7 enclosure. Derived from ../amigahid-pico-case; YAPP MIT library.
// World X = KiCad X, world Y = 60 - KiCad Y. All dimensions in mm.
include <pcb-mechanics.scad>

/* [Output] */
view = "print"; // [print,base,lid,assembled,fit,mounts,menu,collision]
oled = true;
/* [Menu lever] */
menu_lever = true;
menu_x = sw2[0]; // extracted switch position; PCB and opening move together
menu_tip_y = sw2[1]-6.5024; // neutral tip offset from footprint/model datum
menu_slot_width = 15.4;
menu_slot_height = 3.4;
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
mount_bore = 3.4; // clearance for M3 screws through base and PCB
mount_head = 6.4; // 90-degree countersink, measure selected nylon screw heads
recovery_bore = 1.6; // recessed actuator; use a blunt nonconductive probe
// Board top to underside of the OLED PCB, using H1-H4 nylon spacers.
oled_standoff = 12;
oled_board_thickness = 1.2;
oled_glass_height = 2.6;
oled_window_y_trim = 0;
// Inputs used by YAPP overrides must exist before its include.
ox = 2.2 + 3;
oy = 2.2 + pcb_clearance;
board_top = 3 + 3 + board_t;
ceiling = 3 + 6 + 15;
case_x = board_x + 3 + pcb_clearance + 2*2.2;
case_y = board_y + 2*pcb_clearance + 2*2.2;
case_z = ceiling + 2.5;
oled_corner = [h1[0]-2.5, h1[1]+2.55];
window_xy = [oled_corner[0]+17.75, oled_corner[1]-17.4+oled_window_y_trim];
include <vendor/YAPP_Box/YAPPgenerator_v3.scad>

/* [Hidden] */
pcbLength = board_x;
pcbWidth = board_y;
pcbThickness = board_t;
standoffHeight = 3;
standoffDiameter = 6.4;
standoffPinDiameter = 3;
standoffHoleSlack = 0.2;
paddingFront = pcb_clearance;
paddingBack = 3; // plastic wall clear of left-facing RM2 antenna
paddingLeft = pcb_clearance;
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
printLidShell = view != "base" && view != "fit" && view != "mounts";
printSwitchExtenders = false;
printDisplayClips = false;
colorBase = "SlateGray";
colorLid = "Gainsboro";
alphaBase = 1;
alphaLid = 1;

assert(ceiling - (board_top+oled_standoff+oled_board_thickness+oled_glass_height) >= 0.4,
       "OLED glass needs at least 0.4 mm clearance under lid");
assert(port_clearance >= 0.3);
assert(!menu_lever || (menu_x >= 30 && menu_x <= 35),
       "Review snaps, recovery opening and PCB placement outside X=30..35");
assert(menu_slot_width >= 15.4 && menu_slot_height >= 3.4);
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
    [j7[1], 6.4, 0, 0, 7.5, yappCircle, 0, 0, yappCenter, yappCoordPCB],
    [usb_y, 7.85, 15.7+2*port_clearance, 15.7+2*port_clearance,
     0.5, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
];
// +Y wall: full DE9 flange/body relief permits top-down PCB installation.
// Centres come from the two shell mounting pads, NOT the footprint origin.
cutoutsRight = [for (x=de9_centres)
    [x, 6.25, 30.8+2*port_clearance, 12.5+2*port_clearance,
     0.4, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
];
// -Y wall: USB-C service opening and adjacent recessed recovery actuator.
cutoutsLeft = [
    [j10[0], 1.63, 8.94+2*port_clearance, 3.26+2*port_clearance,
     0.7, yappRoundedRect, 0, 0, yappCenter, yappCoordPCB],
    [sw1[0], 1.2, 0, 0, recovery_bore/2, yappCircle,
     0, 0, yappCenter, yappCoordPCB],
    if (menu_lever)
        [menu_x, 1.1, menu_slot_width, menu_slot_height, 0.6,
         yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
];
cutoutsBack = [];
cutoutsLid = oled ? [
    [window_xy[0], window_xy[1], 32.22, 17.5, 0.5,
     yappRoundedRect, 0, 0, yappCenter, yappCoordPCB]
] : [];
// Snap locations avoid the connector apertures and the antenna end.
snapJoins = [[18+oy,6,yappBack], [48+oy,6,yappBack],
             [18+ox,6,yappLeft],
             menu_lever ? [46+ox,3,yappLeft] : [36+ox,6,yappLeft]];
pcbStands = [];
connectors = [];

// H1-H4 support both boards. Screws from below engage the OLED spacers.
// Support edges near connectors without adding PCB mounting holes.
module hookBaseInside() {
    for (h=oled_mounts) translate([paddingBack+h[0],paddingLeft+h[1],-0.01])
        difference() {
            cylinder(d=6.4,h=standoffHeight+0.01,$fn=48);
            translate([0,0,-0.02]) cylinder(d=mount_bore,h=standoffHeight+0.06,$fn=36);
        }
    for (y=[7,32])
        translate([paddingBack+board_x-0.6,paddingLeft+y-1.5,-0.01])
            cube([0.6+paddingFront+0.1,3,standoffHeight+0.01]);
    translate([paddingBack+32.5,paddingLeft+board_y-0.6,-0.01])
        cube([3,0.6+paddingRight+0.1,standoffHeight+0.01]);
    translate([paddingBack+67,paddingLeft-0.1,-0.01])
        cube([2,paddingLeft+0.7,standoffHeight+0.01]);
    // Support the PCB below the switch; keep the moving actuator unobstructed.
    if (menu_lever)
        translate([paddingBack+menu_x-5,paddingLeft-0.7,-0.01])
            cube([10,1.4,standoffHeight+0.01]);
}

// Finger access through the USB-C wall; dimensions are in assembled space.
// The shallow flare leaves the rest of the 2.2 mm wall and roof intact.
module menu_finger_cut() {
    if (menu_lever) hull() {
        translate([ox+menu_x,-0.05,board_top+1.1]) rotate([90,0,0])
            linear_extrude(0.1) offset(r=1)
                square([menu_slot_width+2.6-2,6.4-2],center=true);
        translate([ox+menu_x,1,board_top+1.1]) rotate([90,0,0])
            linear_extrude(0.01) offset(r=0.6)
                square([menu_slot_width-1.2,menu_slot_height-1.2],center=true);
    }
}

// Drawing-based approximation, not a supplier solid model. Includes a generous
// 14.2 mm lateral sweep and the 0.7 mm inward push stroke for clearance checks.
module menu_envelope(sweep=false) {
    if (menu_lever) translate([ox+menu_x,oy+menu_tip_y,board_top]) {
        if (sweep) translate([-7.1,0,0.01]) cube([14.2,9.5,2.19]);
        else {
            color("DimGray") translate([-4.75,4.3,0.01]) cube([9.5,4.5,2.19]);
            color("Ivory") translate([0,6.65,0.2]) linear_extrude(1.8)
                intersection() {
                    difference() { circle(r=6.65,$fn=96); circle(r=5.4,$fn=96); }
                    translate([-7,-7]) square([14,7]);
                }
        }
    }
}

module base_mount_cuts() {
    for (h=oled_mounts) translate([ox+h[0],oy+h[1],-0.02]) {
        cylinder(d=mount_bore,h=basePlaneThickness+standoffHeight+0.04,$fn=48);
        cylinder(d1=mount_head,d2=mount_bore,
                 h=(mount_head-mount_bore)/2+0.02,$fn=48);
    }
    // Drawing worst case: 19.75 - 15.45 = 4.30 mm below the seating plane.
    // Four blind pockets retain 2.2 mm of the 3 mm floor below shield tabs.
    for (tab=usb_shield_tabs)
        translate([ox+tab[0],oy+tab[1],basePlaneThickness-0.8])
            cylinder(d=3.5,h=0.82,$fn=36);
}

// Inspection-only envelopes. Never included in printable base/lid exports.
module pcb_envelope() {
    translate([ox,oy,basePlaneThickness+standoffHeight+0.01]) difference() {
        cube([board_x,board_y,board_t-0.01]);
        translate([-0.01,8,-0.01]) cube([3.61,16,board_t+0.02]);
        for(h=oled_mounts) translate([h[0],h[1],-0.01])
            cylinder(d=3.2,h=board_t+0.02,$fn=32);
    }
}
module hardware_envelopes() {
    color("ForestGreen") pcb_envelope();
    menu_envelope();
    // Supplier full-body envelopes, conservative at connector corners.
    // LCK-205A09FJ2A00B1A: rear 1.38 mm behind pin 1, flange 10.92 mm
    // toward the cable, then a 5.90 mm nose. Use placement, not board depth.
    for(i=[0:1]) let(j=i==0 ? j5 : j6) color("Silver")
        translate([ox+de9_centres[i]-15.4,oy+j[1]-1.38,board_top])
            cube([30.8,18.20,12.5]);
    color("Silver") translate([ox+57.08,oy+j7[1]-7,board_top]) cube([13,14,13]);
    color("Silver") translate([ox+j4[0]-3.2,oy+usb_y-7.3,board_top]) cube([17.3,14.6,15.7]);
    for (tab=usb_shield_tabs) color("Silver")
        translate([ox+tab[0],oy+tab[1],board_top-4.3])
            cylinder(d=2.3,h=4.31,$fn=24);
    color("Silver") translate([ox+j10[0]-4.47,oy+j10[1]-4.99,board_top]) cube([8.94,7.35,3.26]);
    color("DimGray") translate([ox+sw1[0]-2.35,oy+sw1[1]-1.07,board_top]) cube([4.7,1.9,2.35]);
    color("White") translate([ox+sw1[0]-1,oy+sw1[1]-2.43,board_top+0.75]) cube([2,1.36,0.9]);
    color("Black") translate([ox+u1[0]-3.5,oy+u1[1]-3.5,board_top]) cube([7,7,1]);
    color("SeaGreen") translate([ox+u4[0]-10.75,oy+u4[1]-7.25,board_top+0.01]) cube([16.5,14.5,2.6]);
    // 8 mm covers both the polymer family drawing and 7.72 mm supplier model.
    color("DimGray") translate([ox+c4[0],oy+c4[1],board_top])
        cylinder(d=6.6,h=8,$fn=48);
    if(oled) {
        for(h=oled_mounts) color("Ivory")
            translate([ox+h[0],oy+h[1],board_top+0.01])
                difference() {
                    cylinder(d=6.35,h=oled_standoff-0.01,$fn=6);
                    translate([0,0,-0.01]) cylinder(d=3,h=oled_standoff+0.02,$fn=24);
                }
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
    // Broad ribs retain a solid border around the relocated central display.
    difference() {
        for (i=[0:5])
            translate([-0.1,case_y-13-i*rib_pitch-rib_width/2,case_z-rib_depth])
                cube([case_x+0.2,rib_width,rib_depth+0.1]);
        translate([ox+window_xy[0]-18.5,oy+window_xy[1]-11,case_z-1])
            cube([37,22,2]);
    }
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
        if (printBaseShell) { base_mount_cuts(); menu_finger_cut(); }
        if (printLidShell) {
            if ($preview && !showSideBySide) menu_finger_cut();
            else translate([0,2*case_y+10+shiftLid,case_z])
                rotate([180,0,0]) menu_finger_cut();
        }
        if (amiga_style && printLidShell) color(colorLid) {
            if ($preview && !showSideBySide) lid_style_cuts();
            else
                // Match YAPP's face-down, mirrored lid on the print plate.
                translate([0,2*case_y+10+shiftLid,case_z])
                    rotate([180,0,0]) lid_style_cuts();
        }
    }
}

if(view=="collision") intersection() {
    let($preview=true) case_shell();
    union() { hardware_envelopes(); menu_envelope(sweep=true); }
}
else {
    if(view=="menu") color("Gainsboro") render() let($preview=true) case_shell();
    else if(view=="assembled") let($preview=true) case_shell();
    else if(view=="lid") translate([0,-(case_y+10+shiftLid),0]) case_shell();
    else case_shell();
    if(view=="fit") hardware_envelopes();
    if(view=="menu") menu_envelope();
    if(view=="mounts") {
        color([0.1,0.5,0.2,0.35]) pcb_envelope();
        for(h=oled_mounts) color("Ivory")
            translate([ox+h[0],oy+h[1],board_top])
                cylinder(d=6.35,h=oled_standoff,$fn=6);
    }
}
echo("r7 case outside XYZ",[case_x,case_y,case_z]);
echo("r7 PCB top Z / OLED glass top Z",board_top,board_top+oled_standoff+oled_board_thickness+oled_glass_height);
