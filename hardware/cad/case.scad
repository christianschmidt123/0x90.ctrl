// =========================================================================
// 0x90.deck StreamDeck - Elegoo Centauri Carbon Edition (PLA)
// FIXED: Hintere Löcher tiefer gesetzt, alle Löcher näher am Rand (X)
// =========================================================================

$fn = 64;

// --- PARAMETER: SCHALTER & LAYOUT ---
switch_w = 15.0;
switch_h = 15.0;
switch_spacing = 20.0;
plate_thickness = 3.0;

// --- PARAMETER: MATRIX & LAYOUT ---
matrix_cols = 3;
matrix_rows = 3;
profile_count = 2;
profile_offset_y = 10.0;

// --- BERECHNUNG DER INNEN-MATRIX ---
matrix_w = (matrix_cols - 1) * switch_spacing + switch_w;
matrix_h = (matrix_rows - 1) * switch_spacing + switch_h;
profile_area_w = 12.0 + switch_w;

inner_width = matrix_w + profile_area_w + 6.0;
inner_length = max(matrix_h, (profile_count - 1) * switch_spacing + switch_h) + 6.0;

// --- PARAMETER: GEHÄUSE & ERGONOMIE ---
outer_margin = 15.0;
tilt_angle = 45;

plate_width = inner_width + (2 * outer_margin);
plate_length = inner_length + (2 * outer_margin);

case_depth_y = plate_length * cos(tilt_angle);

case_front_height = 28.0;
case_back_height = case_front_height + (plate_length * sin(tilt_angle));

// --- PARAMETER: GEHÄUSEWAND & SCHRAUBEN ---
insert_diameter = 4.8;     // M3 Schmelzhülse
insert_length = 8.0;       // Bohrtiefe für die Hülse
screw_hole_diameter = 3.2; // Loch in Topplate

wall_thickness = insert_diameter + 4.0; // Stabile Wandung

// --- FIX: SEPARATE OFFSETS FÜR PERFEKTE POSITIONIERUNG ---
hole_offset_x = 5.5;       // Weiter an den linken/rechten Rand gerückt (alt: ~7.3)
hole_offset_y_front = 6.0; // Vorne nah am Rand (weil flach)
hole_offset_y_back = 14.5; // Hinten DEUTLICH tiefer gesetzt, damit sie voll im Fleisch sitzen!

hole_positions_top = [
    [hole_offset_x, hole_offset_y_front],                           // Vorne Links
    [plate_width - hole_offset_x, hole_offset_y_front],           // Vorne Rechts
    [hole_offset_x, plate_length - hole_offset_y_back],           // Hinten Links (tiefer)
    [plate_width - hole_offset_x, plate_length - hole_offset_y_back] // Hinten Rechts (tiefer)
];

// Rechnet die schrägen 2D-Koordinaten in globale 3D-Tisch-Koordinaten um
function to_global(pos) = [
    pos[0],
    pos[1] * cos(tilt_angle),
    case_front_height + (pos[1] * sin(tilt_angle))
];

// --- MODULE: TASTEN-AUSSCHNITTE ---
module switch_cutouts() {
    for (col = [0 : matrix_cols - 1]) {
        for (row = [0 : matrix_rows - 1]) {
            x_pos = outer_margin + 3.0 + (col * switch_spacing);
            y_pos = outer_margin + 3.0 + (row * switch_spacing);
            translate([x_pos, y_pos, -1])
                cube([switch_w, switch_h, plate_thickness + 2]);
        }
    }
    for (i = [0 : profile_count - 1]) {
        x_pos = outer_margin + 3.0 + matrix_w + 4.0;
        y_pos = outer_margin + 3.0 + profile_offset_y + (i * switch_spacing);
        translate([x_pos, y_pos, -1])
            cube([switch_w, switch_h, plate_thickness + 2]);
    }
}

// --- MODULE: TOPPLATE ---
module topplate() {
    difference() {
        cube([plate_width, plate_length, plate_thickness]);
        switch_cutouts();

        for (pos = hole_positions_top) {
            translate([pos[0], pos[1], -0.5])
                cylinder(h=plate_thickness + 1, d=screw_hole_diameter);
        }
    }
}

// --- MODULE: BOTTOMCASE ---
module bottomcase() {
    difference() {
        // 1. Der massive Außenkörper
        hull() {
            cube([plate_width, 0.1, case_front_height]);
            translate([0, case_depth_y - 0.1, 0])
                cube([plate_width, 0.1, case_back_height]);
        }

        // 2. Der ausgehöhlte Innenraum (nach oben verlängert, um die Schräge komplett zu durchbrechen)
        hull() {
            translate([wall_thickness, wall_thickness, wall_thickness])
                cube([plate_width - (2 * wall_thickness), 0.1, case_back_height + 50]);
            translate([wall_thickness, case_depth_y - wall_thickness - 0.1, wall_thickness])
                cube([plate_width - (2 * wall_thickness), 0.1, case_back_height + 50]);
        }

        // 3. Exakter Deckel-Ausschnitt oben (damit die Schräge perfekt glatt ist)
        translate([-1, -1, 0])
        rotate([tilt_angle, 0, 0])
        translate([0, 0, case_front_height / cos(tilt_angle)])
            cube([plate_width + 2, plate_length + 2, 100]);

        // 4. Löcher für Schmelzhülsen rechtwinklig zur Schräge gebohrt
        for (pos = hole_positions_top) {
            g_pos = to_global(pos);

            translate([g_pos[0], g_pos[1], g_pos[2]])
                rotate([tilt_angle, 0, 0])
                    translate([0, 0, -insert_length - 0.01])
                        cylinder(h = insert_length + 0.5, d = insert_diameter);
        }

        // 5. USB-C Durchführung
        translate([plate_width/2 - 6, case_depth_y - wall_thickness - 5, wall_thickness + 0.1])
            cube([12, wall_thickness + 10, 6]);
    }
}

// =========================================================================
// --- EXPORT- & ANSICHTS-STEUERUNG ---
// =========================================================================
// Ändere diesen Wert, um die Bauteile einzeln zu rendern und als STL zu speichern:
// "beide"     -> Zeigt Gehäuse und Topplate nebeneinander an (Vorschau)
// "gehaeuse"  -> Zeigt NUR das Gehäuse für den STL-Export
// "platte"    -> Zeigt NUR die Topplate für den STL-Export

ausgabe = "beide"; 

if (ausgabe == "beide") {
    bottomcase();
    translate([plate_width + (plate_length * 1.1), 0, 0])
        color("DarkSlateGray", 0.6) topplate();
        
} else if (ausgabe == "gehaeuse") {
    bottomcase();
    
} else if (ausgabe == "platte") {
    topplate();
}