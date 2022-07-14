//
// FireCAM bottom mount - converting imperial units to mm
//

// Mount width & length (inces)
bw = 4;
bl = 3;

// Bezel height (mm)
bh = 1.6;

// Mounting screw diameter (inches)
mh = 0.125;


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
}