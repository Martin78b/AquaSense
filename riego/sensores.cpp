#include "sensores.h"
#include "config.h"

void iniciarSensores() {
  analogReadResolution(12);                 // lecturas de 0 a 4095
}

// Los capacitivos dan valores más altos en seco y más bajos en mojado
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
