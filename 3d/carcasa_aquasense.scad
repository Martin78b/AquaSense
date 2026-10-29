// ============================================================================
// Carcasa 3D Paramétrica para AquaSense (100% SIN TORNILLOS + Logo en Tapa)
// Compatible con https://ochafik.com/openscad2 y OpenSCAD de escritorio
//
// Actualizado para Breakout Shield de 78 mm x 69 mm:
// - CERO TORNILLOS: ni en las placas ni en la tapa.
// - Las placas calzan por sus bordes exteriores (78x69 mm y 35x32 mm) en
//   esquineros guía en "L" + clips elásticos laterales (no depende de la
//   posición exacta de los agujeros internos de la placa).
// - Logo oficial de AquaSense (assets/logo.svg) grabado en relieve en la tapa.
// - Tapa con cierre a presión (4 pestañas snap-fit internas) y muescas
//   laterales para abrirla fácil con los dedos.
// - Calado para el USB-C del ESP32, agujero para ficha hembra DC 5.5x2.1 mm,
//   cuna elástica para el capacitor de 1000 uF y 6 ranuras pasacables en "U".
// - Se imprime 100% SIN SOPORTES en FDM (PLA / PETG).
// ============================================================================

/* [Qué pieza mostrar / exportar] */
// Elegí "ambas" para previsualizar, o "base" / "tapa" antes de exportar el STL
pieza = "ambas"; // [ambas, base, tapa, cerradas]
// Mostrar las placas electrónicas de referencia en color
mostrar_placas_referencia = true;
// Incluir el logo de AquaSense en la tapa
incluir_logo_en_tapa = true;
// Tamaño del logo en la tapa (mm)
tamano_logo_mm = 58;
// Profundidad del grabado del logo en la tapa (mm, 0.6 a 0.8 mm = 3 a 4 capas)
profundidad_logo_mm = 0.8;

/* [1. Dimensiones Generales de la Caja (mm)] */
largo_interno = 144;     // Eje X (ampliado para el shield de 78mm + ULN2003)
ancho_interno = 98;      // Eje Y (ampliado para el shield de 69mm + salida de cables de borneras)
alto_base_interno = 42;  // Eje Z
espesor_pared = 2.6;     // Pared firme para PETG / PLA
espesor_piso = 2.4;
radio_esquinas = 4.0;
// Holgura para encastres impresos en 3D (0.35 mm para que las placas entren suaves)
holgura_encastre = 0.35;

/* [2. Breakout Shield ESP32 (78 mm x 69 mm)] */
shield_largo = 78.0;     // Largo de la placa (eje X)
shield_ancho = 69.0;     // Ancho de la placa (eje Y)
// Posición del shield respecto a la esquina inferior-izquierda interna
shield_pos_x = 8.0;
shield_pos_y = 14.5;
// Altura del conector USB-C del ESP32 medido desde la cara inferior del shield
altura_usbc_sobre_shield = 15.0;
ancho_calado_usbc = 15.0;
alto_calado_usbc = 9.5;

/* [3. Módulo ULN2003 (Driver de bombas)] */
uln_largo = 35.0;
uln_ancho = 32.0;
// Posición del ULN2003 dentro de la caja (a la derecha del shield de 78mm)
uln_pos_x = 98.0;
uln_pos_y = 18.0;

/* [4. Conectores de Alimentación y Salidas] */
// Diámetro del agujero para el pin hembra DC 5.5mm (8.2 = plástico común, 11.2 = chasis con tuerca)
diametro_jack_dc = 8.2;
// Diámetro de las 6 ranuras pasacables (3 sensores + 3 bombas) en el frente
diametro_pasacable = 5.5;

/* [5. Cunas y Encastres de las Placas] */
altura_base_placa = 6.0; // Altura libre debajo de las placas para las soldaduras
espesor_pcb = 1.75;      // 1.6 mm estándar + margen para que trabe sin forzar

$fn = 48;

// ============================================================================
// CÁLCULOS DERIVADOS
// ============================================================================
L_ext = largo_interno + 2 * espesor_pared;
W_ext = ancho_interno + 2 * espesor_pared;
H_ext = alto_base_interno + espesor_piso;

// Coordenadas del centro del USB-C del ESP32 (en la pared izquierda X=0)
usbc_y = espesor_pared + shield_pos_y + shield_ancho / 2;
usbc_z = espesor_piso + altura_base_placa + altura_usbc_sobre_shield;

// Coordenadas del Jack DC 5.5mm (en la pared derecha X=L_ext, cerca del ULN2003)
jack_dc_y = espesor_pared + uln_pos_y + uln_ancho + 18;
jack_dc_z = espesor_piso + 18;

// ============================================================================
// GEOMETRÍA 2D DEL LOGO OFICIAL DE AQUASENSE (de assets/logo.svg, 256x256)
// ============================================================================

function bezier3(p0, p1, p2, p3, t) =
    pow(1 - t, 3) * p0 +
    3 * pow(1 - t, 2) * t * p1 +
    3 * (1 - t) * pow(t, 2) * p2 +
    pow(t, 3) * p3;

function bezier2(p0, p1, p2, t) =
    pow(1 - t, 2) * p0 +
    2 * (1 - t) * t * p1 +
    pow(t, 2) * p2;

module svg_gota_2d() {
    n = 24;
    pts_der = [ for (i = [0 : n - 1]) bezier3([128, 18], [152, 60], [212, 104], [212, 156], i / n) ];
    pts_arc = [ for (i = [0 : n]) [128 + 84 * cos(i * 180 / n), 156 + 84 * sin(i * 180 / n)] ];
    pts_izq = [ for (i = [1 : n - 1]) bezier3([44, 156], [44, 104], [104, 60], [128, 18], i / n) ];
    polygon(concat(pts_der, pts_arc, pts_izq));
}

module svg_tierra_2d() {
    n = 16;
    q1 = [ for (i = [0 : n - 1]) bezier2([30, 206], [79, 192], [128, 204], i / n) ];
    q2 = [ for (i = [0 : n])     bezier2([128, 204], [177, 216], [226, 200], i / n) ];
    polygon(concat(q1, q2, [[226, 260], [30, 260]]));
}

module svg_brillo_2d() {
    n = 14;
    for (i = [0 : n - 1]) {
        pA = bezier3([78, 132], [80, 108], [96, 84], [112, 62], i / n);
        pB = bezier3([78, 132], [80, 108], [96, 84], [112, 62], (i + 1) / n);
        hull() {
            translate(pA) circle(d = 9.5);
            translate(pB) circle(d = 9.5);
        }
    }
}

module svg_brote_2d() {
    n = 16;
    hull() {
        translate([128, 206]) circle(d = 9.5);
        translate([128, 130]) circle(d = 9.5);
    }
    hoja_izq_1 = [ for (i = [0 : n - 1]) bezier3([128, 164], [102, 164], [84, 148], [80, 120], i / n) ];
    hoja_izq_2 = [ for (i = [0 : n - 1]) bezier3([80, 120], [106, 118], [126, 134], [128, 164], i / n) ];
    polygon(concat(hoja_izq_1, hoja_izq_2));

    hoja_der_1 = [ for (i = [0 : n - 1]) bezier3([128, 144], [130, 114], [152, 94], [184, 94], i / n) ];
    hoja_der_2 = [ for (i = [0 : n - 1]) bezier3([184, 94], [182, 124], [158, 144], [128, 144], i / n) ];
    polygon(concat(hoja_der_1, hoja_der_2));
}

module logo_aquasense_grabado_2d(diametro_mm) {
    escala = diametro_mm / 222.0;
    scale([escala, escala])
    translate([-128, -129]) {
        difference() {
            svg_gota_2d();
            svg_brote_2d();
            svg_brillo_2d();
            difference() {
                intersection() {
                    offset(r = -5) svg_gota_2d();
                    svg_tierra_2d();
                }
                offset(r = 4) hull() {
                    translate([128, 206]) circle(d = 9.5);
                    translate([128, 190]) circle(d = 9.5);
                }
            }
        }
    }
}

// ============================================================================
// MÓDULOS AUXILIARES DE LA CARCASA
// ============================================================================

module caja_redondeada(l, w, h, r) {
    hull() {
        for (x = [r, l - r])
            for (y = [r, w - r])
                translate([x, y, 0])
                    cylinder(r = r, h = h);
    }
}

// Soporte de esquina con borde guía en "L" y chaflán de entrada (sin pines que
// dependan del lugar exacto donde el fabricante perforó los agujeros)
module esquinero_placa_borde(dx_dir, dy_dir, h_apoyo) {
    pared_borde = 2.4;
    largo_brazo = 11.0;
    solapa_apoyo = 3.2; // Cuánto pisa la esquina de la placa sobre el escalón
    alto_borde_guia = 3.5;

    union() {
        // 1. Escalón de apoyo donde descansa la esquina de la placa (deja libre el centro)
        translate([dx_dir > 0 ? -pared_borde : -solapa_apoyo,
                   dy_dir > 0 ? -pared_borde : -solapa_apoyo, 0])
            cube([solapa_apoyo + pared_borde, solapa_apoyo + pared_borde, h_apoyo]);

        // 2. Paredes exteriores en "L" que encajonan la esquina de la placa
        translate([dx_dir > 0 ? -pared_borde : 0,
                   dy_dir > 0 ? -pared_borde : -largo_brazo, 0])
            cube([pared_borde, largo_brazo + pared_borde, h_apoyo + alto_borde_guia]);

        translate([dx_dir > 0 ? -pared_borde : -largo_brazo,
                   dy_dir > 0 ? -pared_borde : 0, 0])
            cube([largo_brazo + pared_borde, pared_borde, h_apoyo + alto_borde_guia]);
    }
}

// Clip flexible lateral con rampa a 45° (se imprime sin soportes y traba el borde del PCB)
module clip_snap_placa(ancho_clip, h_apoyo, dir_y) {
    espesor_brazo = 1.8;
    saliente_traba = 0.9;
    alto_total = h_apoyo + espesor_pcb + 2.8;

    translate([-ancho_clip / 2, 0, 0])
    union() {
        // Columna flexible desde el piso
        translate([0, dir_y > 0 ? -espesor_brazo : 0, 0])
            cube([ancho_clip, espesor_brazo, alto_total]);

        // Escalón de apoyo inferior para la mitad del borde de la placa
        translate([0, dir_y > 0 ? 0 : -2.2, 0])
            cube([ancho_clip, 2.2, h_apoyo]);

        // Diente de encastre con rampa a 45° abajo y arriba (sin voladizos rectos)
        hull() {
            translate([0, dir_y > 0 ? -espesor_brazo : 0, h_apoyo + espesor_pcb])
                cube([ancho_clip, espesor_brazo, 2.4]);
            translate([0, dir_y > 0 ? -0.2 : -saliente_traba, h_apoyo + espesor_pcb + 0.9])
                cube([ancho_clip, espesor_brazo + saliente_traba, 0.7]);
        }
    }
}

// Cuna completa por bordes exteriores (4 esquineros en L + 2 clips elásticos)
module cuna_placa_encastre(largo_pcb, ancho_pcb, h_apoyo) {
    tol = holgura_encastre;
    L = largo_pcb + 2 * tol;
    W = ancho_pcb + 2 * tol;

    translate([-tol, -tol, 0]) {
        // 4 esquineros con bordes guía en L
        translate([0, 0, 0]) esquinero_placa_borde( 1,  1, h_apoyo);
        translate([L, 0, 0]) esquinero_placa_borde(-1,  1, h_apoyo);
        translate([0, W, 0]) esquinero_placa_borde( 1, -1, h_apoyo);
        translate([L, W, 0]) esquinero_placa_borde(-1, -1, h_apoyo);

        // 2 pestañas elásticas (clips) en el centro de los bordes delantero y trasero
        translate([L / 2, 0, 0]) clip_snap_placa(12, h_apoyo,  1);
        translate([L / 2, W, 0]) clip_snap_placa(12, h_apoyo, -1);
    }
}

// Ranura en "U" desde el borde superior para apoyar cables o fichas desde arriba
module ranura_cable_u(diametro, profundidad_z, espesor) {
    hull() {
        translate([0, 0, 0])
            rotate([90, 0, 0])
                cylinder(d = diametro, h = espesor + 2, center = true);
        translate([0, 0, profundidad_z + 5])
            rotate([90, 0, 0])
                cylinder(d = diametro, h = espesor + 2, center = true);
    }
}

// ============================================================================
// BASE DE LA CARCASA (SIN TORNILLOS)
// ============================================================================
module base_carcasa() {
    difference() {
        union() {
            // 1. Casco exterior hueco
            difference() {
                caja_redondeada(L_ext, W_ext, H_ext, radio_esquinas);
                translate([espesor_pared, espesor_pared, espesor_piso])
                    caja_redondeada(largo_interno, ancho_interno, alto_base_interno + 2, max(1, radio_esquinas - espesor_pared));
            }

            // 2. Cuna de encastre para el Breakout Shield ESP32 (78 x 69 mm)
            translate([espesor_pared + shield_pos_x, espesor_pared + shield_pos_y, espesor_piso])
                cuna_placa_encastre(shield_largo, shield_ancho, altura_base_placa);

            // 3. Cuna de encastre para el Módulo ULN2003 (35 x 32 mm)
            translate([espesor_pared + uln_pos_x, espesor_pared + uln_pos_y, espesor_piso])
                cuna_placa_encastre(uln_largo, uln_ancho, altura_base_placa);

            // 4. Cuna elástica para el capacitor de 1000 uF (10mm de diámetro)
            translate([espesor_pared + uln_pos_x + 17, espesor_pared + uln_pos_y + uln_ancho + 18, espesor_piso])
                difference() {
                    cylinder(d = 14.2, h = 8);
                    translate([0, 0, 1]) cylinder(d = 10.4, h = 8);
                    // Corte frontal para que actúe como clip elástico
                    translate([-3, -8, 1]) cube([6, 8, 8]);
                }
        }

        // --- MUESCAS INTERNAS PARA EL ENCASTRE DE LA TAPA (SNAP-FIT) ---
        for (x_snap = [L_ext * 0.25, L_ext * 0.75]) {
            // Pared frontal (Y = espesor_pared)
            translate([x_snap, espesor_pared, H_ext - 3.2])
                rotate([45, 0, 0])
                    cube([16, 1.8, 1.8], center = true);
            // Pared trasera (Y = W_ext - espesor_pared)
            translate([x_snap, W_ext - espesor_pared, H_ext - 3.2])
                rotate([45, 0, 0])
                    cube([16, 1.8, 1.8], center = true);
        }

        // Muescas en los laterales (X=0 y X=L_ext) para meter la uña y destapar fácil
        translate([-1, W_ext / 2 - 10, H_ext - 1.6])
            cube([espesor_pared + 2, 20, 2.0]);
        translate([L_ext - espesor_pared - 1, W_ext / 2 - 10, H_ext - 1.6])
            cube([espesor_pared + 2, 20, 2.0]);

        // --- CALADOS Y AGUJEROS DE CONECTORES ---

        // A. Calado para USB-C del ESP32 (pared izquierda, X = 0)
        hull() {
            for (dy = [-(ancho_calado_usbc - alto_calado_usbc)/2, (ancho_calado_usbc - alto_calado_usbc)/2])
                translate([-1, usbc_y + dy, usbc_z])
                    rotate([0, 90, 0])
                        cylinder(d = alto_calado_usbc, h = espesor_pared + 10);
        }

        // B. Agujero para el enchufe hembra DC 5.5x2.1mm (pared derecha, X = L_ext)
        translate([L_ext - espesor_pared - 1, jack_dc_y, jack_dc_z])
            rotate([0, 90, 0])
                cylinder(d = diametro_jack_dc, h = espesor_pared + 4);

        // C. 3 Ranuras frontales (Y = 0) para los 3 Sensores Capacitivos
        for (i = [0 : 2]) {
            pos_x_sensor = espesor_pared + shield_pos_x + 18 + i * 21;
            translate([pos_x_sensor, espesor_pared / 2, H_ext - 7])
                ranura_cable_u(diametro_pasacable, 7, espesor_pared + 4);
        }

        // D. 3 Ranuras frontales (Y = 0) para las 3 Bombas de agua
        for (i = [0 : 2]) {
            pos_x_bomba = espesor_pared + uln_pos_x + 6 + i * 11.5;
            translate([pos_x_bomba, espesor_pared / 2, H_ext - 7])
                ranura_cable_u(diametro_pasacable, 7, espesor_pared + 4);
        }
    }
}

// ============================================================================
// PESTAÑA SNAP-FIT PARA LA TAPA
// ============================================================================
module pestana_tapa_snap(ancho_p, alto_labio, dir_y) {
    saliente = 0.85;
    translate([-ancho_p / 2, 0, 0])
    union() {
        // Brazo flexible
        translate([0, dir_y > 0 ? -1.8 : 0, 0])
            cube([ancho_p, 1.8, alto_labio]);
        // Lomo redondeado a 45° que hace "clic" en la muesca de la base
        hull() {
            translate([0, dir_y > 0 ? -1.8 : 0, alto_labio - 2.6])
                cube([ancho_p, 1.8, 2.2]);
            translate([0, dir_y > 0 ? -1.8 - saliente : saliente, alto_labio - 1.7])
                cube([ancho_p, 1.8, 0.6]);
        }
    }
}

// ============================================================================
// TAPA DE LA CARCASA (ENCastre SNAP-FIT + LOGO OFICIAL AQUASENSE)
// ============================================================================
module tapa_carcasa() {
    tol = holgura_encastre;
    alto_labio = 4.8;

    difference() {
        union() {
            // 1. Placa superior
            caja_redondeada(L_ext, W_ext, espesor_piso, radio_esquinas);

            // 2. Labio perimetral de alineación
            translate([espesor_pared + tol, espesor_pared + tol, espesor_piso])
                difference() {
                    caja_redondeada(largo_interno - 2*tol, ancho_interno - 2*tol, 3.0, max(1, radio_esquinas - espesor_pared));
                    translate([1.8, 1.8, -1])
                        caja_redondeada(largo_interno - 2*tol - 3.6, ancho_interno - 2*tol - 3.6, 5.0, 1);
                    // Cortes alrededor de las 4 pestañas snap-fit para que flexionen libremente
                    for (x_snap = [L_ext * 0.25, L_ext * 0.75]) {
                        translate([x_snap - espesor_pared - tol - 9, -2, -1])
                            cube([18, 6, 6]);
                        translate([x_snap - espesor_pared - tol - 9, ancho_interno - 2*tol - 4, -1])
                            cube([18, 6, 6]);
                    }
                }

            // 3. 4 Pestañas elásticas (Snap-Fit) que traban la tapa sin tornillos
            for (x_snap = [L_ext * 0.25, L_ext * 0.75]) {
                // Frente
                translate([x_snap, espesor_pared + tol + 1.8, espesor_piso])
                    pestana_tapa_snap(14, alto_labio, 1);
                // Fondo
                translate([x_snap, W_ext - espesor_pared - tol - 1.8, espesor_piso])
                    pestana_tapa_snap(14, alto_labio, -1);
            }

            // 4. Topes que cierran las 6 ranuras en "U" de los cables al tapar
            for (i = [0 : 2]) {
                pos_x_sensor = espesor_pared + shield_pos_x + 18 + i * 21;
                translate([pos_x_sensor - (diametro_pasacable - 0.5)/2, tol, espesor_piso])
                    cube([diametro_pasacable - 0.5, espesor_pared - tol, 4.2]);

                pos_x_bomba = espesor_pared + uln_pos_x + 6 + i * 11.5;
                translate([pos_x_bomba - (diametro_pasacable - 0.5)/2, tol, espesor_piso])
                    cube([diametro_pasacable - 0.5, espesor_pared - tol, 4.2]);
            }
        }

        // A. LOGO OFICIAL DE AQUASENSE (grabado en la cara exterior Z=0)
        if (incluir_logo_en_tapa) {
            translate([espesor_pared + shield_pos_x + shield_largo / 2 + 6, W_ext / 2, -0.05])
                linear_extrude(height = profundidad_logo_mm + 0.05)
                    logo_aquasense_grabado_2d(tamano_logo_mm);
        }

        // B. Ventana visor sobre los 4 LEDs (A, B, C, D) del ULN2003
        y_visor_en_tapa = W_ext - (espesor_pared + uln_pos_y + 3 + 7);
        translate([espesor_pared + uln_pos_x + 7, y_visor_en_tapa, -1])
            cube([20, 7, espesor_piso + 2]);

        // C. Rejillas de ventilación laterales
        for (i = [0 : 2]) {
            translate([12 + i * 6.5, W_ext / 2 - 20, -1])
                cube([3.2, 40, espesor_piso + 2]);
            translate([espesor_pared + uln_pos_x + 6 + i * 8, 18, -1])
                cube([3.5, 30, espesor_piso + 2]);
        }
    }
}

// ============================================================================
// MODELOS DE REFERENCIA (SOLO PARA VISUALIZAR EN PANTALLA)
// ============================================================================
module placas_referencia() {
    z_pcb = espesor_piso + altura_base_placa;

    // Breakout Shield (78 x 69 mm)
    color("#2c3e50", 0.85)
    translate([espesor_pared + shield_pos_x, espesor_pared + shield_pos_y, z_pcb])
        cube([shield_largo, shield_ancho, 1.6]);

    // ESP32 montado arriba del shield con su puerto USB-C apuntando a la izquierda
    color("#16a085", 0.9)
    translate([espesor_pared + shield_pos_x + 4, espesor_pared + shield_pos_y + (shield_ancho - 28)/2, z_pcb + 11])
        cube([51.5, 28, 1.6]);

    // Conector USB-C del ESP32
    color("silver")
    translate([espesor_pared + shield_pos_x, espesor_pared + shield_pos_y + (shield_ancho - 9)/2, z_pcb + 12.6])
        cube([7.5, 9, 3.2]);

    // Módulo ULN2003
    color("#2980b9", 0.85)
    translate([espesor_pared + uln_pos_x, espesor_pared + uln_pos_y, z_pcb])
        cube([uln_largo, uln_ancho, 1.6]);

    // Capacitor 1000uF en su cuna elástica
    color("#8e44ad", 0.85)
    translate([espesor_pared + uln_pos_x + 17, espesor_pared + uln_pos_y + uln_ancho + 18, espesor_piso + 1])
        cylinder(d = 10, h = 20);
}

// ============================================================================
// RENDERIZADO PRINCIPAL
// ============================================================================
if (pieza == "base") {
    base_carcasa();
    if (mostrar_placas_referencia) %placas_referencia();
} else if (pieza == "tapa") {
    tapa_carcasa();
} else if (pieza == "cerradas") {
    base_carcasa();
    if (mostrar_placas_referencia) %placas_referencia();
    translate([0, W_ext, H_ext + espesor_piso])
        rotate([180, 0, 0])
            tapa_carcasa();
} else {
    // "ambas" lado a lado, listas para imprimir o inspeccionar
    base_carcasa();
    if (mostrar_placas_referencia) %placas_referencia();
    translate([0, W_ext + 12, 0])
        tapa_carcasa();
}
