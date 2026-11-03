// Riego con ESP32 (NodeMCU ESP-32S) + ULN2003 + 3 bombas + web local + Telegram
// Abrí http://riego.local o la IP que muestra el monitor serie (115200 baudios).
// La configuración se guarda en la memoria del ESP32 y sobrevive a cortes de luz.
//
// Requiere la librería "ArduinoJson" (de Benoit Blanchon, versión 7), que se
// instala desde Herramientas > Administrar bibliotecas en el Arduino IDE.
// Las credenciales del WiFi van en secrets.h (copiá secrets.example.h).

#if __has_include("secrets.h")
#include "secrets.h"
#else
// Si no existe secrets.h (por ejemplo en builds genéricos o GitHub Actions),
// se usarán las credenciales ya guardadas en la memoria NVS del ESP32.
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "config.h"
#include "control_riego.h"
#include "sensores.h"
#include "comandos.h"
#include "telegram.h"
#include "servidor_web.h"
#include "utilidades.h"

void setup() {
  iniciarBombas();                          // antes que cualquier otra cosa
  iniciarSensores();

  Serial.begin(115200);
  delay(500);
  Serial.println("\nRiego con web iniciando...");

  iniciarColasTelegram();
  cargarConfig();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (wifiSsid.length() > 0) {
    Serial.printf("Conectando a WiFi: %s\n", wifiSsid.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  } else {
    Serial.println("Sin WiFi en NVS. Intentando caché interna del ESP32...");
    WiFi.begin();
  }

  Serial.print("Buscando red");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiSsid.length() == 0 && WiFi.SSID().length() > 0) {
      guardarWifi(WiFi.SSID(), WiFi.psk());
    }
    Serial.print(" conectado. Web en http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" sin conexión (sigue intentando)");
  }
  if (MDNS.begin(NOMBRE_MDNS)) Serial.printf("También en http://%s.local\n", NOMBRE_MDNS);

  configTzTime(ZONA_HORARIA, NTP_1, NTP_2);

  iniciarServidorWeb();
  iniciarTareaTelegram();
  if (tg.avisarInicio) notificar("🔌 El sistema de riego se encendió. Si no lo reiniciaste vos, puede haber habido un corte de luz.");

  leerSensores();
}

void loop() {
  atenderServidorWeb();
  gestionarRiego();

  // Comandos recibidos por Telegram
  String cmd;
  while (recibirComando(cmd)) procesarComando(cmd);

  static unsigned long ultimaLectura = 0, ultimaLogica = 0, ultimaIp = 0;
  if (millis() - ultimaLectura > 2000) { ultimaLectura = millis(); leerSensores(); }
  if (millis() - ultimaLogica > 10000) { ultimaLogica = millis(); logicaAutomatica(); }
  if (millis() - ultimaIp > 300000) {
    ultimaIp = millis();
    Serial.printf("%s | IP %s\n", horaTexto().c_str(), WiFi.localIP().toString().c_str());
  }

  revisarBoton();                           // botón BOOT: riega todas
}
