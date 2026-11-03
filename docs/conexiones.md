# Conexiones con un alargador de 2 puertos USB

Variante del circuito del [README](../README.md#conexiones) para alimentar todo desde un alargador con dos puertos USB: uno para el ESP32 y otro para las bombas. Los pines salen de [`riego/config.h`](../riego/config.h).

## Alimentación

| Puerto | Alimenta | Cómo |
|---|---|---|
| USB 1 | ESP32 | Cable micro USB, el mismo que se usa para flashear |
| USB 2 | Bombas, a través del ULN2003 | Cable USB viejo cortado: **rojo = +5 V**, **negro = GND**. El verde y el blanco se aíslan con cinta. También sirve un breakout USB-A |

- Nunca funcionan dos bombas a la vez, así que con 1 A en el puerto 2 alcanza.
- No alimentes las bombas desde el pin 5V o VIN del ESP32: si la corriente de los motores pasa por la placa, el ESP32 se reinicia.

## Diagrama

```
 USB 1 ──micro USB──► ESP32                    USB 2 (cable cortado)
                                                 rojo +5V ──┬──► ULN2003 "+"
 ESP32 GPIO 25 ──────────► ULN2003 IN1                      ├──► + de las 3 bombas (rojo)
 ESP32 GPIO 26 ──────────► ULN2003 IN2           [1000µF]   │   (jumper puesto)
 ESP32 GPIO 27 ──────────► ULN2003 IN3            + a "+"   │
 ESP32 GND ──────────────► ULN2003 "−" ◄─────────negro GND ─┘
                           (masas unidas, imprescindible)

 ULN2003, conector blanco de 5 pines:
   (el pin común C+ queda libre; las bombas toman +5 V directo de la fuente)
   A ──► − bomba 1    B ──► − bomba 2    C ──► − bomba 3

 Sensores:
   ESP32 3V3 ──► VCC de los 3 sensores
   ESP32 GND ──► GND de los 3 sensores
   GPIO 32 / 33 / 34 ◄── AOUT sensor 1 / 2 / 3
```

## En la protoboard

- **Rieles separados:** un par para 3,3 V y GND (sensores) y el otro para 5 V y GND (bombas). Los dos GND se unen; el 3V3 y el 5 V, nunca.
- **Capacitor:** la pata larga (+) va al "+" del ULN2003 y la de la franja (−) al "−".
- **Bombas:** el cable rojo (+) va directo al positivo (+5 V) de la fuente y el negro (−) a las salidas A, B o C del ULN2003 (el pin común C+ del conector blanco queda sin usar). Al revés giran al revés y casi no bombean.
- **Ancho de la placa:** el NodeMCU de 38 pines tapa casi una protoboard común. Usá dos protoboards lado a lado, o dejá el ESP32 afuera y conectalo con cables dupont hembra-macho.

## Orden de armado y prueba

1. **Solo el ULN2003, sin bombas:** IN1 a IN3, masas unidas y USB 2. En `http://riego.local`, **Regar** en la maceta 1 tiene que prender el LED del canal A. Repetir con las macetas 2 y 3.
2. **Una bomba en un vaso con agua** (nunca en seco). Después las otras dos.
3. **Sensores al final:** vienen desactivados en el firmware. Calibralos siguiendo el README ("2. Calibrá los sensores").

## Sensores capacitivos V2.0.0 (tipo TLC555)

- Al ser versión **V2.0.0** con el integrado CMOS **TLC555** (a veces marcado "T5C" o "TLC555I"), funcionan de forma nativa y estable alimentados con **3,3 V**. El TLC555 opera desde 2 V hasta 15 V, a diferencia de los clones problemáticos v1.2 con NE555 bipolar que requerían 4,5 V o más.
- **Alimentación directa:** se conectan a **3V3 y GND** del ESP32. Su salida analógica (AOUT) entrega valores entre ~1,2 V (mojado) y ~2,8–3,0 V (seco en el aire), quedando perfectamente dentro del rango de medición de los pines analógicos del ESP32 (GPIO 32, 33 y 34) sin riesgo de sobretensión ni necesidad de adaptadores ni divisores.
- **Protección contra la humedad:** clavá el sensor en la tierra solo hasta la línea blanca marcada ("enterrar hasta la línea"). Para evitar que la humedad degrade la placa con el tiempo, sellá los bordes cortados del PCB (donde la fibra de vidrio puede absorber agua) y la zona de componentes electrónicos superior con barniz/esmalte de uñas transparente o termocontraíble.
