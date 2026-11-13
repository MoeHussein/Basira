// BASIRA - exterior concept model (visual intent only, not a fabrication model)
// Matches the rev 9 design spec: ~150 x 60 x 45 mm visor, two optical modules
// 64 mm apart, teal PD wheel on the bridge, top pod with LED, side vertex-distance
// windows, charcoal face gasket and strap with a teal quick-release buckle.
// Axes: x = width, y = depth (-y faces the chart, +y faces the patient), z = up.

view = "exterior";   // "exterior" or "module_exploded"

PD   = 64;           // optical centre spacing
eyeZ = 3;            // optical centre height on the visor

WHITE    = [0.95, 0.96, 0.96];
LIGHT    = [0.84, 0.88, 0.87];
GRAPHITE = [0.24, 0.27, 0.30];
CHARCOAL = [0.20, 0.26, 0.30];
TEAL     = [0.06, 0.46, 0.43];
BRASS    = [0.72, 0.56, 0.32];
LENS_T   = [0.55, 0.85, 0.82, 0.55];   // plus-cylinder tint
LENS_O   = [0.95, 0.62, 0.45, 0.55];   // minus-cylinder tint
GLASS    = [0.80, 0.93, 0.95, 0.45];
PCB      = [0.12, 0.42, 0.25];

function wrap(x) = 0.0025*x*x;          // visor wraps back toward the temples

// ---------------- exterior ----------------
module visor_shell() {
    difference() {
        hull() for (x=[-63,-31,0,31,63]) for (z=[-17,17]) for (y=[-8,8])
            translate([x, y + wrap(x), z]) sphere(r=12, $fn=48);
        // nose arch
        hull() {
            translate([0, 0, -31]) rotate([90,0,0]) cylinder(r=15, h=80, center=true, $fn=64);
            translate([0, 0, -16]) rotate([90,0,0]) cylinder(r=7,  h=80, center=true, $fn=48);
        }
        // module seats
        for (s=[-1,1]) translate([s*PD/2, -24 + wrap(PD/2), eyeZ]) rotate([-90,0,0]) cylinder(r=20.5, h=10, $fn=96);
    }
}

module optical_module() {        // seen from the front (-y)
    rotate([90,0,0]) {           // local z -> world -y (toward the chart)
        color(LIGHT) difference() { cylinder(r=21, h=6, $fn=120); translate([0,0,-1]) cylinder(r=17.5, h=8, $fn=120); }
        color(WHITE) difference() { translate([0,0,0.5]) cylinder(r=17.5, h=6, $fn=120); translate([0,0,-1]) cylinder(r=15.3, h=9, $fn=120); }
        color(GRAPHITE) for (a=[0:10:350]) rotate(a) translate([15.8, -0.25, 6.4]) cube([(a%30==0)?1.6:0.9, 0.5, 0.25]);
        color(GRAPHITE) difference() {
            translate([0,0,0.8]) cylinder(r=15.3, h=5, $fn=120);
            translate([0,0,-1]) cylinder(r=12, h=8, $fn=120);
            for (a=[0:7.5:359]) rotate(a) translate([12.8, -0.4, 5.2]) cube([3, 0.8, 1]);
        }
        color(GLASS) translate([0,0,3.4]) cylinder(r=12, h=1.2, $fn=120);
        color([0.08,0.10,0.12]) translate([0,0,-4]) cylinder(r=12, h=1, $fn=96);
        color([1,1,1,0.7]) translate([0,0,4.7]) difference() {
            cylinder(r=10, h=0.2, $fn=96); translate([1.8,-1.8,-1]) cylinder(r=10, h=2, $fn=96);
        }
    }
}

module pd_wheel() {
    translate([0, -20, 15]) {
        color(TEAL) rotate([0,90,0]) difference() {
            cylinder(r=6.5, h=11, center=true, $fn=64);
            for (a=[0:15:359]) rotate(a) translate([6.3,-0.5,-6]) cube([1,1,12]);
        }
    }
    color(GRAPHITE) for (i=[-4:4]) translate([i*1.8 - 0.2, -21.0, 4.5 - ((i%2==0)?1.5:0.8)]) cube([0.4, 0.4, (i%2==0)?3:1.6]);
}

module top_pod() {
    color(LIGHT) hull() for (x=[-17,17]) for (y=[-4,10]) for (z=[30,35]) translate([x,y,z]) sphere(r=4, $fn=32);
    color(TEAL) translate([15, -8.2, 33]) sphere(r=1.5, $fn=24);
    color([0.70,0.74,0.73]) translate([-4, -8.1, 32.6]) rotate([90,0,0])
        linear_extrude(0.5) text("BASIRA", size=3.6, font="Liberation Sans:style=Bold", halign="center", valign="center");
}

module side_window(s) {
    translate([s*75.2, -1, 0]) rotate([0,0,s>0?0:180]) {
        color(GLASS) translate([0,-8,-4.5]) cube([0.8, 16, 9]);
        color(GRAPHITE) for (i=[0:7]) translate([0.8, -7 + i*2, (i%2==0)?-3.5:-2]) cube([0.2, 0.3, (i%2==0)?7:4]);
    }
}

module motor_bumps() {
    color(LIGHT) for (s=[-1,1]) for (dz=[-12,14])
        translate([s*58, -15 + wrap(58), eyeZ + dz]) scale([0.9,0.6,1.1]) sphere(r=8, $fn=40);
}

module gasket() {
    color(CHARCOAL) difference() {
        hull() for (x=[-60,-28,0,28,60]) for (z=[-14,14])
            translate([x, 16 + wrap(x), z]) rotate([-90,0,0]) cylinder(r=11, h=9, $fn=40);
        hull() {
            translate([0, 0, -31]) rotate([90,0,0]) cylinder(r=16.5, h=200, center=true, $fn=64);
            translate([0, 0, -15]) rotate([90,0,0]) cylinder(r=8.5,  h=200, center=true, $fn=48);
        }
        for (s=[-1,1]) translate([s*PD/2, 30, eyeZ]) rotate([90,0,0]) scale([1.25,1,1]) cylinder(r=14, h=20, $fn=64);
    }
}

module strap() {
    color(CHARCOAL) intersection() {
        translate([0, 32, eyeZ]) scale([1, 1.25, 1]) rotate_extrude($fn=160)
            translate([79, 0]) square([3, 22], center=true);
        translate([-200, 22, -100]) cube([400, 400, 200]);
    }
    color(TEAL) translate([-80.8, 60, eyeZ]) rotate([0,0,-24]) hull() for (y=[-5,5]) for (z=[-8,8])
        translate([0,y,z]) rotate([0,90,0]) cylinder(r=2.5, h=4, center=true, $fn=24);
}

module exterior() {
    color(WHITE) visor_shell();
    for (s=[-1,1]) translate([s*PD/2, -21.8 + wrap(PD/2), eyeZ]) optical_module();
    pd_wheel(); top_pod(); gasket(); strap();
    side_window(1); side_window(-1);
}

// ---------------- exploded optical module ----------------
module badge(n, p) {
    translate(p) {
        color(WHITE) rotate([0,90,0]) cylinder(r=5, h=1, $fn=48);
        color(GRAPHITE) translate([0.9,0,0]) rotate([90,0,90])
            linear_extrude(0.6) text(str(n), size=5.5, halign="center", valign="center");
    }
}
module ring_with_lens(tint) {
    rotate([-90,0,0]) {
        color(GRAPHITE) difference() { cylinder(r=17, h=6, $fn=96); translate([0,0,-1]) cylinder(r=13.5, h=8, $fn=96); }
        color(GRAPHITE) for (a=[0:9:359]) rotate(a) translate([16.6,-0.7,0.5]) cube([1.6,1.4,5]);
        color(tint) translate([0,0,2]) cylinder(r=13.5, h=2, $fn=96);
    }
}
module micro_motor(p) {
    translate(p) {
        color([0.55,0.58,0.60]) translate([-5,-6,-5]) cube([10,12,10]);
        color(GRAPHITE) translate([-5,-6,-17]) cube([10,12,12]);
        color(PCB) translate([6,-5,-12]) cube([1.2,10,10]);
    }
}
module module_exploded() {
    // 1 face gasket (eye side)
    color(CHARCOAL) rotate([-90,0,0]) difference() { cylinder(r=22, h=6, $fn=96); translate([0,0,-1]) cylinder(r=14, h=8, $fn=96); }
    // 2 Alvarez sphere plates with racks, pinion and guide pins
    translate([0, 26, 0]) {
        color(LENS_T) translate([-12, 0, -12]) cube([24, 2.5, 30]);
        color(LENS_T) translate([-12, 4, -18]) cube([24, 2.5, 30]);
        color([0.65,0.67,0.69]) { translate([12, 0, -12]) cube([2, 2.5, 30]); translate([12, 4, -18]) cube([2, 2.5, 30]); }
        color([0.75,0.77,0.79]) for (x=[-15,15]) translate([x, 3.2, -24]) cylinder(r=1, h=46, $fn=24);
        color(BRASS) translate([16, 3.2, -21]) rotate([0,90,0]) cylinder(r=4, h=3, $fn=32);
        micro_motor([22, 3.2, -24]);
    }
    // 3 minus cylinder, 4 plus cylinder, each in a geared ring with its motor and sensor
    translate([0, 56, 0]) { ring_with_lens(LENS_O); micro_motor([0, 3, -24]); }
    translate([0, 82, 0]) { ring_with_lens(LENS_T); micro_motor([0, 3, -24]); }
    // 6 occluder flap with its servo
    translate([0, 104, 0]) {
        color([0.20,0.23,0.26,0.75]) translate([-15, 0, -18]) cube([30, 1.5, 34]);
        color(GRAPHITE) translate([15, -3, 4]) cube([8, 7, 12]);
    }
    // 7 front window and white cover
    translate([0, 122, 0]) rotate([-90,0,0]) {
        color(GLASS) translate([0,0,2]) cylinder(r=16.6, h=1, $fn=96);
        color(WHITE) difference() { cylinder(r=23, h=5, $fn=96); translate([0,0,-1]) cylinder(r=16, h=7, $fn=96); }
    }
    // number badges (legend on the slide)
    badge(1, [26, -4, 30]);
    badge(2, [26, 20, 30]);
    badge(3, [26, 50, 30]);
    badge(4, [26, 76, 30]);
    badge(5, [34, 48, -32]);
    badge(6, [26, 98, 30]);
    badge(7, [26, 118, 30]);
}

if (view == "exterior") exterior();
if (view == "module_exploded") module_exploded();
