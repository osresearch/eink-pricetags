/*
 * Pogo-pin programmer for the MSP430 and serial console.
 */


base_w = 53.0;
base_h = 27.3;
base_r = 2;
base_t = 6;
base_t2 = 4;

pogo_x1 = 25.9;
pogo_y1 =  7.0;
pogo_x2 = 26.9;
pogo_y2 = 16.0;
pogo_r = 1.0;


module cube_round(w,h,t,r1,r2=9)
{
	hull() {
		translate([r1,r1,0]) cylinder(r1=r1, r2=r2, h=t, $fn=30);
		translate([r1,h-r1,0]) cylinder(r1=r1, r2=r2, h=t, $fn=30);
		translate([w-r1,r1,0]) cylinder(r1=r1, r2=r2, h=t, $fn=30);
		translate([w-r1,h-r1,0]) cylinder(r1=r1, r2=r2, h=t, $fn=30);
	}
}

function inch(x) = x * 25.4;

//%cube_round(base_w, base_h, base_t, base_r);

module quad_pogo(x,y)
{
	translate([x + inch(0.0), y + inch(0.0), -1]) cylinder(d=pogo_r, h=20, $fn=30);
	translate([x + inch(0.1), y + inch(0.0), -1]) cylinder(d=pogo_r, h=20, $fn=30);
	translate([x + inch(0.0), y + inch(0.1), -1]) cylinder(d=pogo_r, h=20, $fn=30);
	translate([x + inch(0.1), y + inch(0.1), -1]) cylinder(d=pogo_r, h=20, $fn=30);
}

render() difference()
{
	translate([0,0,base_t]) mirror([0,0,1])
	cube_round(base_w, base_h, base_t, base_r, base_r-0.5);

	quad_pogo(pogo_x1, pogo_y1);
	quad_pogo(pogo_x2, pogo_y2);

	translate([pogo_x1-2, pogo_y1 + 3.5, base_t-1]) linear_extrude(height=2) text("R T", size=3);
	translate([pogo_x1-2, pogo_y1 - 3.5, base_t-1]) linear_extrude(height=2) text("V G", size=3);

	translate([base_t2, base_t2, -1]) cube_round(pogo_x1 - base_t2*1.5, base_h - 2*base_t2, base_t + 2, base_r, base_r);
	translate([pogo_x1 + 5, base_t2, -1]) cube_round(base_w - pogo_x1 - base_t2 - 5, base_h - 2*base_t2, base_t + 2, base_r, base_r);
	//translate([base_t, base_t, -1]) cube_round(base_w - 2*base_t, base_h - 2*base_t, base_t + 2, base_r, base_r);

	//translate([base_r/2, base_h - 4, base_t-0.5]) linear_extrude(height=2) text("e-ink programmer", size=3);
}

