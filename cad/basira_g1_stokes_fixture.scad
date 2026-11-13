// BASIRA - gate G1: Stokes-pair test fixture for a lensmeter (no motors)
// Holds two trial-lens cylinders (+3.00 DC and -3.00 DC) on one axis.
//   Ring 1 is read against the BASE pointer        -> beta  (common axis)
//   Ring 2 is read against the RING-1 pointer      -> Delta (relative angle)
//   alpha = beta + Delta.  Compare lensmeter SPH/CYL/AX with rev 9 section 3.
// Units: mm.  Tested with OpenSCAD 2021.01.
//
// BEFORE PRINTING: measure the borrowed trial lenses with calipers and
// replace the five "Lens" numbers below.  Then export each part:
//   set part = "base", F6 (render), File > Export > STL; repeat for "ring1", "ring2".
// Print: PLA or PETG, 0.2 mm layers, 4 walls, 30 % infill, no supports,
// each part flat with its engraved scale facing UP.  A light colour helps;
// run a fine marker over the engraved ticks.

part = "all";              // "all" (exploded preview), "base", "ring1", "ring2"

/* [Lens - measure these] */
lens_od    = 38.0;   // trial-lens rim outer diameter
rim_t      = 3.0;    // rim thickness = pocket depth (handle must not be thicker)
handle_w   = 8.0;    // handle width; 0 if the lens has no handle
handle_len = 20.0;   // handle length from the rim edge outward
bulge      = 1.5;    // how far glass stands proud of the rim face (clearance)

/* [Print fit] */
clr      = 0.3;      // clearance per side around the lens and handle
rclr     = 0.5;      // diametral clearance for the rotating rings
aperture = 32;       // clear hole through all parts

/* [Layout] */
base_t   = 6;   base_pocket  = 3;   // ring 1 sits in this pocket
ring1_t  = 8;   ring1_pocket = 3;   // ring 2 sits in this pocket
ring2_t  = 6;
flat_gap = 2;                       // wall left at the straight edge
engrave  = 0.6;                     // depth of ticks and numbers

handle_r = lens_od/2 + handle_len + 1;        // outer end of the handle slot
ring2_d  = 2*(handle_r + 12);
ring1_d  = ring2_d + 24;
base_d   = ring1_d + 20;
flat_y   = ring1_d/2 + rclr + flat_gap;       // straight edge = 0-180 deg line

assert(rim_t + 2 <= ring1_t - ring1_pocket, "ring 1 too thin for this rim");
assert(rim_t + bulge + 1 <= ring2_t,        "ring 2 too thin for this rim");

// ---------- 2D helpers ----------
module knurl_disc(d, n=96, depth=0.8) {
    difference() {
        circle(d=d, $fn=360);
        for (i=[0:n-1]) rotate(i*360/n) translate([d/2, 0]) circle(r=depth, $fn=12);
    }
}
module ticks2d(r_out, step=2) {
    for (a=[0:step:359]) {
        tl = (a % 10 == 0) ? 4.0 : 2.2;
        rotate(a) translate([r_out-tl, -0.25]) square([tl, 0.5]);
    }
}
module labels2d(r, step=30, size=3.2) {
    for (a=[0:step:359]) rotate(a) translate([r, 0]) rotate(-90)
        text(str(a), size=size, halign="center", valign="center");
}
module pointer2d(r_tip, len=4) {             // triangle at 0 deg, tip pointing inward
    polygon([[r_tip, 0], [r_tip+len, -1.6], [r_tip+len, 1.6]]);
}
module keyhole2d() {                          // lens rim + radial handle slot (at 90 deg)
    circle(d=lens_od + 2*clr, $fn=180);
    if (handle_w > 0)
        rotate(90) translate([0, -(handle_w + 2*clr)/2]) square([handle_r, handle_w + 2*clr]);
}

// ---------- parts ----------
module base() {
    difference() {
        intersection() {
            cylinder(d=base_d, h=base_t, $fn=240);
            translate([-base_d/2, -flat_y, -1]) cube([base_d, base_d, base_t+2]);
        }
        translate([0, 0, -1]) cylinder(d=aperture, h=base_t+2, $fn=120);
        translate([0, 0, base_t-base_pocket]) cylinder(d=ring1_d+rclr, h=base_pocket+1, $fn=240);
        translate([0, 0, base_t-engrave]) linear_extrude(engrave+1) {
            pointer2d(ring1_d/2 + rclr/2 + 0.8);
        }
    }
}

module ring1() {
    floor_z = ring1_t - ring1_pocket - rim_t;
    difference() {
        linear_extrude(ring1_t) knurl_disc(ring1_d);
        translate([0, 0, -1]) cylinder(d=aperture, h=ring1_t+2, $fn=120);
        translate([0, 0, ring1_t-ring1_pocket]) cylinder(d=ring2_d+rclr, h=ring1_pocket+1, $fn=240);
        translate([0, 0, floor_z]) linear_extrude(rim_t+0.01) keyhole2d();
        translate([0, 0, ring1_t-engrave]) linear_extrude(engrave+1) {
            ticks2d(ring1_d/2 - 1.6);                    // beta scale
            labels2d(ring1_d/2 - 1.6 - 4 - 3.2);
            pointer2d(ring2_d/2 + rclr/2 + 0.6, 3);      // Delta pointer for ring 2
        }
    }
}

module ring2() {
    difference() {
        linear_extrude(ring2_t) knurl_disc(ring2_d);
        translate([0, 0, -1]) cylinder(d=aperture, h=ring2_t+2, $fn=120);
        translate([0, 0, -0.01]) cylinder(d=lens_od + 2*clr + 1, h=bulge, $fn=180);   // clears lens 1
        translate([0, 0, ring2_t-rim_t]) linear_extrude(rim_t+0.01) keyhole2d();
        translate([0, 0, ring2_t-engrave]) linear_extrude(engrave+1) {
            ticks2d(ring2_d/2 - 1.2);                    // Delta scale
            labels2d(ring2_d/2 - 1.2 - 4 - 3.4);
        }
    }
}

// ---------- output ----------
if (part == "base")  base();
if (part == "ring1") ring1();
if (part == "ring2") ring2();
if (part == "all") {
    color("Gainsboro") base();
    color("LightSteelBlue") translate([0, 0, base_t - base_pocket + 25]) ring1();
    color("PowderBlue") translate([0, 0, base_t - base_pocket + ring1_t - ring1_pocket + 50]) ring2();
}
echo(str("BASIRA G1 sizes (mm): base ", base_d, " (flat edge at y=-", flat_y,
         "), ring1 ", ring1_d, ", ring2 ", ring2_d, ", handle slot to r=", handle_r));
