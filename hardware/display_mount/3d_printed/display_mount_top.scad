//
// FireCAM display bezel top part - converting imperial units to mm
//

// Bezel width & length (inces)
bw = 4;
bl = 3;

// Bezel height (mm)
bh = 1.5;

// Mounting screw diameter (inches)
mh = 0.125;

// Antenna mount diameter (inches)
ah = 0.375;

module hole(x, y, d) {
    translate([x * 25.4, y * 25.4, -1]) {
        cylinder(h = bh+2, r = (d * 25.4)/2, $fn=120);
    }
}


difference() {
    union() {
        // Plate
        cube([bw * 25.4, bl * 25.4, bh]);
        
        // Label
        translate([bw/2 * 25.4, (bl - 0.45) * 25.4, 0]) {
            linear_extrude(height = bh + 1) {
                text(text = "FireCAM", size=7, halign = "center");
            }
        }
    }
    
    // PCB Mounting holes
    hole(0.125, 0.125, mh);
    hole(0.125, 2.85, mh);
    hole(3.35, 0.125, mh);
    hole(3.35, 2.85, mh);
    
    // LCD Screen cutout
    translate([0.4 * 25.4, 0.375 * 25.4, -1]) {
        cube([2.78 * 25.4, 1.985 * 25.4, bh + 2]);
    }
    
    // Antenna mounting hole
    hole(3.7, 2.625, ah);
}