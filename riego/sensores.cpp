#include "sensores.h"
#include "config.h"

void iniciarSensores() {
  analogReadResolution(12);                 // lecturas de 0 a 4095
}

// Los sensores capacitivos V2.0.0 (TLC555 a 3,3 V) dan valores más altos en seco
// (típicamente ~2800) y más bajos en mojado (típicamente ~1200).
void leerSensores() {
  for (int i = 0; i < N; i++) {
    long suma = 0;
    for (int j = 0; j < 8; j++) suma += analogRead(PIN_SENSOR[i]);
    mac[i].raw = suma / 8;
    if (mac[i].adcSeco == mac[i].adcMojado) {
      mac[i].pct = 0;
    } else {
      mac[i].pct = constrain(map(mac[i].raw, mac[i].adcSeco, mac[i].adcMojado, 0, 100), 0, 100);
    }
  }
}
