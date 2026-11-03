#include "servidor_web.h"
#include "config.h"
#include "control_riego.h"
#include "pagina_web.h"
#include "telegram.h"
#include "utilidades.h"
#include "ota.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(80);

static String k(const char* a, int i) { return String(a) + i; }

static int argInt(const String& clave, int def, int mn, int mx) {
  if (!server.hasArg(clave)) return def;
  return constrain(server.arg(clave).toInt(), mn, mx);
}

static float argFloat(const String& clave, float def, float mn, float mx) {
  if (!server.hasArg(clave)) return def;
  return constrain(server.arg(clave).toFloat(), mn, mx);
}

static void handlePagina() {
  server.send_P(200, "text/html", PAGINA);
}

static void handleEstado() {
  char estadoTg[100], desconocido[24];
  tgBloquear();
  strlcpy(estadoTg, tgEstado, sizeof(estadoTg));
  strlcpy(desconocido, tgDesconocido, sizeof(desconocido));
  tgLiberar();

  String j = "{\"hora\":\"";
  j += horaTexto();
  j += "\",\"rssi\":";
  j += (int)WiFi.RSSI();
  j += ",\"regando\":";
  j += regando;
  j += ",\"restante\":";
  j += segundosRestantes();
  j += ",\"pausa\":";
  j += pausa ? "true" : "false";
  j += ",\"tgEstado\":\"";
  j += jsonEsc(estadoTg);
  j += "\",\"tgDesconocido\":\"";
  j += jsonEsc(desconocido);
  j += "\",\"macetas\":[";
  for (int i = 0; i < N; i++) {
    if (i) j += ",";
    j += "{\"nombre\":\"";  j += jsonEsc(mac[i].nombre);
    j += "\",\"raw\":";     j += mac[i].raw;
    j += ",\"pct\":";       j += mac[i].pct;
    j += ",\"sensor\":";    j += mac[i].usarSensor ? "true" : "false";
    j += ",\"riegosHoy\":"; j += mac[i].riegosHoy;
    j += ",\"mlHoy\":";     j += mac[i].mlHoy;
    j += ",\"ml\":";        j += mac[i].ml;
    j += ",\"pendiente\":"; j += mac[i].pendiente ? "true" : "false";
    j += ",\"ultimo\":\"";  j += jsonEsc(mac[i].ultimoRiego);
    j += "\"}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleConfigGet() {
  String j = "{\"maxs\":";
  j += maxSegundos;
  j += ",\"horarios\":[";
  for (int h = 0; h < MAX_HORARIOS; h++) {
    if (h) j += ",";
    j += "{\"a\":"; j += hor[h].activo ? "true" : "false";
    j += ",\"h\":"; j += hor[h].h;
    j += ",\"m\":"; j += hor[h].m;
    j += "}";
  }
  j += "],\"tg\":{\"on\":";        j += tg.activo ? "true" : "false";
  j += ",\"tokenGuardado\":";      j += tg.token.length() ? "true" : "false";   // el token nunca se envía
  j += ",\"chat\":\"";             j += jsonEsc(tg.chatId);
  j += "\",\"cmd\":";              j += tg.comandos ? "true" : "false";
  j += ",\"riego\":";              j += tg.avisarRiego ? "true" : "false";
  j += ",\"max\":";                j += tg.avisarMaximo ? "true" : "false";
  j += ",\"ini\":";                j += tg.avisarInicio ? "true" : "false";
  j += ",\"res\":";                j += tg.resumen ? "true" : "false";
  j += ",\"rh\":";                 j += tg.resumenH;
  j += ",\"rm\":";                 j += tg.resumenM;
  j += "},\"macetas\":[";
  for (int i = 0; i < N; i++) {
    if (i) j += ",";
    j += "{\"nombre\":\""; j += jsonEsc(mac[i].nombre);
    j += "\",\"ml\":";     j += mac[i].ml;
    j += ",\"caudal\":";   j += String(mac[i].caudal, 1);
    j += ",\"sensor\":";   j += mac[i].usarSensor ? "true" : "false";
    j += ",\"umbral\":";   j += mac[i].umbral;
    j += ",\"seco\":";     j += mac[i].adcSeco;
    j += ",\"mojado\":";   j += mac[i].adcMojado;
    j += ",\"espera\":";   j += mac[i].esperaMin;
    j += ",\"maxDia\":";   j += mac[i].maxDia;
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleConfigPost() {
  maxSegundos = argInt("maxs", maxSegundos, 1, 300);
  for (int h = 0; h < MAX_HORARIOS; h++) {
    hor[h].activo = argInt(k("ha", h), hor[h].activo, 0, 1);
    hor[h].h      = argInt(k("hh", h), hor[h].h, 0, 23);
    hor[h].m      = argInt(k("hm", h), hor[h].m, 0, 59);
  }
  for (int i = 0; i < N; i++) {
    if (server.hasArg(k("n", i))) {
      String n = server.arg(k("n", i));
      n.replace("\"", "");
      n.replace("\\", "");
      n.trim();
      if (n.length() > 0) mac[i].nombre = n.substring(0, LARGO_NOMBRE);
    }
    mac[i].ml         = argInt(k("ml", i), mac[i].ml, 10, 3000);
    mac[i].caudal     = argFloat(k("q", i), mac[i].caudal, 1.0, 200.0);
    mac[i].usarSensor = argInt(k("s", i), mac[i].usarSensor, 0, 1);
    mac[i].umbral     = argInt(k("u", i), mac[i].umbral, 0, 100);
    mac[i].adcSeco    = argInt(k("as", i), mac[i].adcSeco, 0, 4095);
    mac[i].adcMojado  = argInt(k("am", i), mac[i].adcMojado, 0, 4095);
    mac[i].esperaMin  = argInt(k("e", i), mac[i].esperaMin, 0, 1440);
    mac[i].maxDia     = argInt(k("x", i), mac[i].maxDia, 0, 24);
  }

  tgBloquear();
  tg.activo = argInt("tgOn", tg.activo, 0, 1);
  if (server.hasArg("tgTok")) {
    String t = server.arg("tgTok");
    t.trim();
    if (t.length() > 0 && t.length() < 64) tg.token = t;   // vacío: se mantiene el guardado
  }
  if (server.hasArg("tgChat")) {
    String c = server.arg("tgChat"), limpio;
    for (unsigned int i = 0; i < c.length(); i++) {
      if (isdigit((uint8_t)c[i]) || (c[i] == '-' && limpio.length() == 0)) limpio += c[i];
    }
    tg.chatId = limpio.substring(0, 20);
    if (tg.chatId.length() && strcmp(tgDesconocido, tg.chatId.c_str()) == 0) tgDesconocido[0] = 0;
  }
  tg.comandos     = argInt("tgCmd", tg.comandos, 0, 1);
  tg.avisarRiego  = argInt("nRie", tg.avisarRiego, 0, 1);
  tg.avisarMaximo = argInt("nMax", tg.avisarMaximo, 0, 1);
  tg.avisarInicio = argInt("nIni", tg.avisarInicio, 0, 1);
  tg.resumen      = argInt("nRes", tg.resumen, 0, 1);
  tg.resumenH     = argInt("rH", tg.resumenH, 0, 23);
  tg.resumenM     = argInt("rM", tg.resumenM, 0, 59);
  tgReconfigurar = true;
  tgLiberar();

  guardarConfig();
  Serial.println("Ajustes guardados desde la web");
  server.send(200, "text/plain", "ok");
}

static void handleRegar() {
  int m = argInt("m", -1, -1, N - 1);
  if (m >= 0) encolar(m, 'w');
  server.send(200, "text/plain", "ok");
}

static void handleDetener() {
  detenerTodo();
  server.send(200, "text/plain", "ok");
}

static void handlePausa() {
  pausa = argInt("on", pausa, 0, 1);
  guardarPausa();
  server.send(200, "text/plain", "ok");
}

static void handlePrueba() {
  if (!tg.activo || tg.token.length() == 0) {
    server.send(400, "text/plain", "Activá Telegram, pegá el token y guardá los ajustes antes de probar.");
    return;
  }
  if (tg.chatId.length() == 0) {
    server.send(400, "text/plain", "Falta autorizar tu chat. Escribile a tu bot y tocá Autorizar en esta pestaña.");
    return;
  }
  notificar("✅ Mensaje de prueba del sistema de riego. Las notificaciones funcionan.");
  server.send(200, "text/plain", "Mensaje en camino. Si no te llega en unos segundos, mirá el estado de la conexión.");
}

static void handleWifiPost() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Falta el nombre de la red WiFi.");
    return;
  }
  String s = server.arg("ssid");
  s.trim();
  if (s.length() == 0) {
    server.send(400, "text/plain", "El nombre de la red no puede estar vacío.");
    return;
  }
  String p = server.hasArg("pass") ? server.arg("pass") : "";
  guardarWifi(s, p);
  Serial.printf("WiFi actualizado desde la web: %s. Reiniciando...\n", s.c_str());
  server.send(200, "text/plain", "Red WiFi guardada en NVS. Reiniciando el ESP32 para conectar...");
  delay(1000);
  ESP.restart();
}

void iniciarServidorWeb() {
  server.on("/", HTTP_GET, handlePagina);
  server.on("/api/estado", HTTP_GET, handleEstado);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/regar", HTTP_POST, handleRegar);
  server.on("/api/detener", HTTP_POST, handleDetener);
  server.on("/api/pausa", HTTP_POST, handlePausa);
  server.on("/api/telegram/prueba", HTTP_POST, handlePrueba);
  server.on("/api/wifi", HTTP_POST, handleWifiPost);
  iniciarOTA(server);
  server.begin();
}

void atenderServidorWeb() {
  server.handleClient();
}
