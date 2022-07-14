//
// FireCAM display bezel bottom part - converting imperial units to mm
//

// Bezel width & length (inces)
bw = 4;
bl = 3;

// Bezel height (mm)
bh = 1.6;

// Mounting screw diameter (inches)
mh = 0.125;

// Antenna mount diameter (inches)
ah = 0.250;

module hole(x, y, d) {
    translate([x * 25.4, y * 25.4, -1]) {
        cylinder(h = bh+2, r = (d * 25.4)/2, $fn=120);
    }
}


difference() {
    // Plate
    cube([bw * 25.4, bl * 25.4, bh]);
    
    // PCB Mounting holes
    hole(0.125, 0.125, mh);
    hole(0.125, 2.85, mh);
    hole(3.35, 0.125, mh);
    hole(3.35, 2.85, mh);
    
    // LCD Mounting holes
    hole(0.25, 0.5, mh);
    hole(0.25, 2.232, mh);
    hole(3.245, 0.5, mh);
    hole(3.245, 2.232, mh);
    
    // LCD circuitry cutout
    translate([0.4 * 25.4, 0.616 * 25.4, -1]) {
        cube([3.1 * 25.4, 1.5 * 25.4, bh + 2]);
    }
    
    // LCD SD Card connector cutout
    translate([1.14 * 25.4, 0.375 * 25.4, -1]) {
        cube([1.07 * 25.4, 0.5 * 25.4, bh + 2]);
    }
    
    // Antenna mounting hole
    hole(3.7, 2.625, ah);
}