// Riego con ESP32 (NodeMCU ESP-32S) + ULN2003 + 3 bombas + web local + Telegram
// Abrí http://riego.local o la IP que muestra el monitor serie (115200 baudios).
// La configuración se guarda en la memoria del ESP32 y sobrevive a cortes de luz.
//
// Requiere la librería "ArduinoJson" (de Benoit Blanchon, versión 7), que se
// instala desde Herramientas > Administrar bibliotecas en el Arduino IDE.

#define ARDUINOJSON_USE_LONG_LONG 1
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ===================== DATOS FIJOS =====================
const char* WIFI_SSID    = "TU_RED";
const char* WIFI_PASS    = "TU_CLAVE";
const char* ZONA_HORARIA = "<-03>3";       // Argentina (UTC-3)
const char* NOMBRE_MDNS  = "riego";        // http://riego.local

const int N = 3;
const int PIN_BOMBA[N]  = {25, 26, 27};    // IN1, IN2, IN3 del ULN2003
const int PIN_SENSOR[N] = {32, 33, 34};    // sensores de humedad (ADC1)
const int BOTON = 0;                       // botón BOOT: riega todas
const int MAX_HORARIOS = 4;
const unsigned long PAUSA_ENTRE_BOMBAS_MS = 2000;
// =======================================================

struct Maceta {
  // configuración
  String nombre;
  int ml;            // ml por riego
  float caudal;      // ml por segundo de la bomba
  bool usarSensor;
  int umbral;        // % de humedad debajo del cual riega
  int adcSeco;       // lectura con tierra seca
  int adcMojado;     // lectura con tierra mojada
  int esperaMin;     // minutos mínimos entre riegos automáticos
  int maxDia;        // riegos automáticos máximos por día
  // estado
  int riegosHoy = 0;
  int mlHoy = 0;
  int mlEnCurso = 0;
  bool yaRego = false;
  unsigned long ultimoRiegoMs = 0;
  String ultimoRiego = "todavía no";
  bool pendiente = false;
  char origen = 'w';
  int raw = 0;
  int pct = 0;
};

struct Horario { bool activo; int h; int m; int ultimoDia; };

struct ConfigTelegram {
  bool activo = false;
  String token;
  String chatId;
  bool comandos = true;
  bool avisarRiego = true;
  bool avisarMaximo = true;
  bool avisarInicio = true;
  bool resumen = true;
  int resumenH = 21;
  int resumenM = 0;
};

Maceta mac[N];
Horario hor[MAX_HORARIOS];
ConfigTelegram tg;
int maxSegundos = 60;
bool pausa = false;

WebServer server(80);
Preferences prefs;

int regando = -1;
unsigned long inicioRiego = 0, duracionRiego = 0, finUltimoRiego = 0;
int diaActual = -1;
int diaResumen = -1;
unsigned long ultimoResetSinHora = 0;

// ---- Comunicación entre el programa principal y la tarea de Telegram ----
struct MsgTg { char texto[600]; };
struct CmdTg { char texto[80]; };
QueueHandle_t colaSalida;                  // mensajes a enviar
QueueHandle_t colaEntrada;                 // comandos recibidos
SemaphoreHandle_t mtx;                     // protege tg y los textos de estado
char tgEstado[100] = "Desactivado";
char tgDesconocido[24] = "";
volatile bool tgReconfigurar = true;

String k(const char* a, int i) { return String(a) + i; }

// ---------------- Configuración ----------------
void valoresPorDefecto() {
  const char* nombres[N] = {"Hortensia A", "Hortensia B", "Kiwi C"};
  const int ml[N]     = {400, 400, 150};
  const int umbral[N] = {55, 55, 38};
  const int espera[N] = {120, 120, 60};
  const int maxDia[N] = {3, 3, 2};
  for (int i = 0; i < N; i++) {
    mac[i].nombre = nombres[i];
    mac[i].ml = ml[i];
    mac[i].caudal = 20.0;
    mac[i].usarSensor = false;
    mac[i].umbral = umbral[i];
    mac[i].adcSeco = 2800;
    mac[i].adcMojado = 1200;
    mac[i].esperaMin = espera[i];
    mac[i].maxDia = maxDia[i];
  }
  hor[0] = {true, 8, 0, -1};
  hor[1] = {true, 19, 0, -1};
  hor[2] = {false, 13, 0, -1};
  hor[3] = {false, 22, 0, -1};
  maxSegundos = 60;
}

void cargarConfig() {
  valoresPorDefecto();
  prefs.begin("riego", true);
  if (prefs.getBool("ok", false)) {
    maxSegundos = prefs.getInt("maxs", maxSegundos);
    for (int i = 0; i < N; i++) {
      mac[i].nombre     = prefs.getString(k("n", i).c_str(), mac[i].nombre);
      mac[i].ml         = prefs.getInt(k("ml", i).c_str(), mac[i].ml);
      mac[i].caudal     = prefs.getFloat(k("q", i).c_str(), mac[i].caudal);
      mac[i].usarSensor = prefs.getBool(k("s", i).c_str(), mac[i].usarSensor);
      mac[i].umbral     = prefs.getInt(k("u", i).c_str(), mac[i].umbral);
      mac[i].adcSeco    = prefs.getInt(k("as", i).c_str(), mac[i].adcSeco);
      mac[i].adcMojado  = prefs.getInt(k("am", i).c_str(), mac[i].adcMojado);
      mac[i].esperaMin  = prefs.getInt(k("e", i).c_str(), mac[i].esperaMin);
      mac[i].maxDia     = prefs.getInt(k("x", i).c_str(), mac[i].maxDia);
    }
    for (int h = 0; h < MAX_HORARIOS; h++) {
      hor[h].activo = prefs.getBool(k("ha", h).c_str(), hor[h].activo);
      hor[h].h      = prefs.getInt(k("hh", h).c_str(), hor[h].h);
      hor[h].m      = prefs.getInt(k("hm", h).c_str(), hor[h].m);
    }
  }
  tg.activo       = prefs.getBool("tgOn", tg.activo);
  tg.token        = prefs.getString("tgTok", tg.token);
  tg.chatId       = prefs.getString("tgChat", tg.chatId);
  tg.comandos     = prefs.getBool("tgCmd", tg.comandos);
  tg.avisarRiego  = prefs.getBool("nRie", tg.avisarRiego);
  tg.avisarMaximo = prefs.getBool("nMax", tg.avisarMaximo);
  tg.avisarInicio = prefs.getBool("nIni", tg.avisarInicio);
  tg.resumen      = prefs.getBool("nRes", tg.resumen);
  tg.resumenH     = prefs.getInt("rH", tg.resumenH);
  tg.resumenM     = prefs.getInt("rM", tg.resumenM);
  pausa           = prefs.getBool("pausa", false);
  prefs.end();
}

void guardarConfig() {
  prefs.begin("riego", false);
  prefs.putInt("maxs", maxSegundos);
  for (int i = 0; i < N; i++) {
    prefs.putString(k("n", i).c_str(), mac[i].nombre);
    prefs.putInt(k("ml", i).c_str(), mac[i].ml);
    prefs.putFloat(k("q", i).c_str(), mac[i].caudal);
    prefs.putBool(k("s", i).c_str(), mac[i].usarSensor);
    prefs.putInt(k("u", i).c_str(), mac[i].umbral);
    prefs.putInt(k("as", i).c_str(), mac[i].adcSeco);
    prefs.putInt(k("am", i).c_str(), mac[i].adcMojado);
    prefs.putInt(k("e", i).c_str(), mac[i].esperaMin);
    prefs.putInt(k("x", i).c_str(), mac[i].maxDia);
  }
  for (int h = 0; h < MAX_HORARIOS; h++) {
    prefs.putBool(k("ha", h).c_str(), hor[h].activo);
    prefs.putInt(k("hh", h).c_str(), hor[h].h);
    prefs.putInt(k("hm", h).c_str(), hor[h].m);
  }
  prefs.putBool("tgOn", tg.activo);
  prefs.putString("tgTok", tg.token);
  prefs.putString("tgChat", tg.chatId);
  prefs.putBool("tgCmd", tg.comandos);
  prefs.putBool("nRie", tg.avisarRiego);
  prefs.putBool("nMax", tg.avisarMaximo);
  prefs.putBool("nIni", tg.avisarInicio);
  prefs.putBool("nRes", tg.resumen);
  prefs.putInt("rH", tg.resumenH);
  prefs.putInt("rM", tg.resumenM);
  prefs.putBool("ok", true);
  prefs.end();
}

void guardarPausa() {
  prefs.begin("riego", false);
  prefs.putBool("pausa", pausa);
  prefs.end();
}

// ---------------- Utilidades ----------------
String horaTexto() {
  struct tm t;
  if (!getLocalTime(&t, 10)) return "sin hora";
  char b[20];
  snprintf(b, sizeof(b), "%02d/%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  return String(b);
}

String dosDigitos(int n) { return n < 10 ? "0" + String(n) : String(n); }

String jsonEsc(const String& s) {
  String o;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

String urlEncode(const char* s) {
  String o;
  char b[4];
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      o += (char)c;
    } else {
      snprintf(b, sizeof(b), "%%%02X", c);
      o += b;
    }
  }
  return o;
}

const char* origenTexto(char o) {
  switch (o) {
    case 'h': return "por horario";
    case 's': return "por sensor";
    case 't': return "desde Telegram";
    case 'b': return "con el botón";
    default:  return "desde la web";
  }
}

// Pone un mensaje en la cola de salida de Telegram (no bloquea)
void notificar(const String& texto) {
  if (!tg.activo) return;
  MsgTg m;
  strlcpy(m.texto, texto.c_str(), sizeof(m.texto));
  xQueueSend(colaSalida, &m, 0);
}

// ---------------- Sensores ----------------
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

// ---------------- Riego ----------------
void apagarBombas() {
  for (int i = 0; i < N; i++) digitalWrite(PIN_BOMBA[i], LOW);
}

void detenerTodo() {
  apagarBombas();
  if (regando >= 0) Serial.printf("Riego de %s detenido\n", mac[regando].nombre.c_str());
  regando = -1;
  finUltimoRiego = millis();
  for (int i = 0; i < N; i++) mac[i].pendiente = false;
}

void iniciarRiego(int i) {
  apagarBombas();                           // nunca dos bombas a la vez
  float caudal = mac[i].caudal < 0.1f ? 0.1f : mac[i].caudal;
  float seg = mac[i].ml / caudal;
  if (seg > maxSegundos) seg = maxSegundos;
  duracionRiego = (unsigned long)(seg * 1000);
  inicioRiego = millis();
  regando = i;
  mac[i].pendiente = false;
  mac[i].mlEnCurso = (int)(seg * caudal + 0.5f);
  mac[i].mlHoy += mac[i].mlEnCurso;
  mac[i].yaRego = true;
  mac[i].ultimoRiegoMs = millis();
  mac[i].ultimoRiego = horaTexto();
  Serial.printf("Regando %s: %d ml (%.1f s)\n", mac[i].nombre.c_str(), mac[i].mlEnCurso, seg);
  digitalWrite(PIN_BOMBA[i], HIGH);
}

void gestionarRiego() {
  if (regando >= 0) {
    if (millis() - inicioRiego >= duracionRiego) {
      int i = regando;
      digitalWrite(PIN_BOMBA[i], LOW);
      Serial.printf("%s lista\n", mac[i].nombre.c_str());
      regando = -1;
      finUltimoRiego = millis();
      if (tg.avisarRiego) {
        notificar(String("💧 ") + mac[i].nombre + " regada: " + mac[i].mlEnCurso + " ml (" + origenTexto(mac[i].origen) + ").");
      }
    }
    return;
  }
  if (millis() - finUltimoRiego < PAUSA_ENTRE_BOMBAS_MS) return;
  for (int i = 0; i < N; i++) {
    if (mac[i].pendiente) { iniciarRiego(i); return; }
  }
}

bool encolar(int i, char origen) {
  if (mac[i].pendiente || regando == i) return false;
  mac[i].pendiente = true;
  mac[i].origen = origen;
  return true;
}

void encolarAutomatico(int i, char origen) {
  if (mac[i].pendiente || regando == i) return;
  if (mac[i].riegosHoy >= mac[i].maxDia) return;
  mac[i].riegosHoy++;
  encolar(i, origen);
  if (mac[i].usarSensor && mac[i].riegosHoy >= mac[i].maxDia && tg.avisarMaximo) {
    notificar(String("⚠️ ") + mac[i].nombre + " llegó al máximo de " + mac[i].maxDia +
              " riegos automáticos de hoy. Si la tierra sigue seca, revisá el sensor y la manguera.");
  }
}

void reiniciarContadores() {
  for (int i = 0; i < N; i++) { mac[i].riegosHoy = 0; mac[i].mlHoy = 0; }
}

String textoResumen();

void logicaAutomatica() {
  struct tm t;
  bool hayHora = getLocalTime(&t, 10);

  // Contadores diarios
  if (hayHora && t.tm_yday != diaActual) {
    diaActual = t.tm_yday;
    reiniciarContadores();
  } else if (!hayHora && millis() - ultimoResetSinHora > 86400000UL) {
    ultimoResetSinHora = millis();
    reiniciarContadores();
  }

  // Resumen diario por Telegram
  if (hayHora && tg.resumen && t.tm_hour == tg.resumenH && t.tm_min == tg.resumenM && diaResumen != t.tm_yday) {
    diaResumen = t.tm_yday;
    notificar(textoResumen());
  }

  // Horarios: solo para macetas sin sensor
  if (hayHora) {
    for (int h = 0; h < MAX_HORARIOS; h++) {
      if (hor[h].activo && t.tm_hour == hor[h].h && t.tm_min == hor[h].m && hor[h].ultimoDia != t.tm_yday) {
        hor[h].ultimoDia = t.tm_yday;
        if (pausa) continue;
        Serial.printf("Horario %02d:%02d\n", hor[h].h, hor[h].m);
        for (int i = 0; i < N; i++) if (!mac[i].usarSensor) encolarAutomatico(i, 'h');
      }
    }
  }

  // Sensores: riega si está seca y ya pasó la espera mínima
  if (pausa) return;
  for (int i = 0; i < N; i++) {
    if (!mac[i].usarSensor) continue;
    bool esperaCumplida = !mac[i].yaRego || millis() - mac[i].ultimoRiegoMs >= mac[i].esperaMin * 60000UL;
    if (mac[i].pct < mac[i].umbral && esperaCumplida) encolarAutomatico(i, 's');
  }
}

// ---------------- Textos para Telegram ----------------
String textoAyuda() {
  return String("🌱 Comandos del riego\n\n") +
         "/estado: cómo están las macetas\n" +
         "/regar 1, /regar kiwi o /regar todas: riega con la cantidad configurada\n" +
         "/detener: corta el riego en curso\n" +
         "/pausa: frena el riego automático (los manuales siguen andando)\n" +
         "/reanudar: retoma el riego automático\n" +
         "/horarios: muestra los horarios de riego\n" +
         "/ayuda: muestra esta lista";
}

String textoEstado() {
  String s = "🌱 Estado del riego (" + horaTexto() + ")\n";
  if (pausa) s += "⏸ El riego automático está en pausa.\n";
  for (int i = 0; i < N; i++) {
    s += "\n" + String(i + 1) + ". " + mac[i].nombre + ": ";
    if (mac[i].usarSensor) s += "humedad " + String(mac[i].pct) + " % (riega por debajo de " + String(mac[i].umbral) + " %)";
    else s += "riego por horario";
    s += "\n   Hoy: " + String(mac[i].mlHoy) + " ml. Último riego: " + mac[i].ultimoRiego;
    if (regando == i) s += "\n   💧 Regando ahora";
    else if (mac[i].pendiente) s += "\n   En espera para regar";
  }
  return s;
}

String textoHorarios() {
  String s = "🕒 Horarios (para las macetas sin sensor):\n";
  bool alguno = false;
  for (int h = 0; h < MAX_HORARIOS; h++) {
    if (!hor[h].activo) continue;
    s += "\n• " + dosDigitos(hor[h].h) + ":" + dosDigitos(hor[h].m);
    alguno = true;
  }
  if (!alguno) s += "\nNo hay horarios activos.";
  String conSensor;
  for (int i = 0; i < N; i++) {
    if (!mac[i].usarSensor) continue;
    if (conSensor.length()) conSensor += ", ";
    conSensor += mac[i].nombre;
  }
  if (conSensor.length()) s += "\n\nRiegan según el sensor: " + conSensor;
  if (pausa) s += "\n\n⏸ El riego automático está en pausa.";
  return s;
}

String textoResumen() {
  String s = "📋 Resumen del día\n";
  for (int i = 0; i < N; i++) {
    s += "\n" + mac[i].nombre + ": " + String(mac[i].mlHoy) + " ml regados";
    if (mac[i].usarSensor) s += ", humedad actual " + String(mac[i].pct) + " %";
  }
  if (pausa) s += "\n\n⏸ El riego automático está en pausa.";
  return s;
}

void procesarComando(String t) {
  t.trim();
  if (!t.startsWith("/")) {
    notificar("Mandá /ayuda para ver los comandos disponibles.");
    return;
  }
  int esp = t.indexOf(' ');
  String cmd = esp < 0 ? t : t.substring(0, esp);
  String arg = esp < 0 ? "" : t.substring(esp + 1);
  int arroba = cmd.indexOf('@');             // en grupos llega como /estado@MiBot
  if (arroba > 0) cmd = cmd.substring(0, arroba);
  cmd.toLowerCase();
  arg.trim();
  arg.toLowerCase();
  Serial.printf("Comando de Telegram: %s %s\n", cmd.c_str(), arg.c_str());

  if (cmd == "/start" || cmd == "/ayuda" || cmd == "/help") {
    notificar(textoAyuda());
  } else if (cmd == "/estado") {
    notificar(textoEstado());
  } else if (cmd == "/horarios") {
    notificar(textoHorarios());
  } else if (cmd == "/detener") {
    detenerTodo();
    notificar("⏹ Riego detenido.");
  } else if (cmd == "/pausa") {
    pausa = true;
    guardarPausa();
    notificar("⏸ Riego automático en pausa. Los riegos manuales siguen funcionando. Mandá /reanudar para retomarlo.");
  } else if (cmd == "/reanudar") {
    pausa = false;
    guardarPausa();
    notificar("▶️ Riego automático reanudado.");
  } else if (cmd == "/regar") {
    if (arg.length() == 0) {
      notificar("Indicá qué maceta regar, por ejemplo: /regar 1, /regar kiwi o /regar todas.");
    } else if (arg == "todas") {
      for (int i = 0; i < N; i++) encolar(i, 't');
      notificar("💧 Riego todas las macetas, de a una.");
    } else {
      int i = -1;
      if (arg.length() == 1 && arg[0] >= '1' && arg[0] < '1' + N) {
        i = arg[0] - '1';
      } else {
        for (int j = 0; j < N; j++) {
          String n = mac[j].nombre;
          n.toLowerCase();
          if (n.indexOf(arg) >= 0) { i = j; break; }
        }
      }
      if (i < 0) notificar("No encontré esa maceta. Usá su número (1 a " + String(N) + "), parte del nombre o todas.");
      else if (!encolar(i, 't')) notificar(mac[i].nombre + " ya se está regando o está en espera.");
      else notificar("💧 Riego " + mac[i].nombre + ": " + String(mac[i].ml) + " ml.");
    }
  } else {
    notificar("No conozco ese comando. Mandá /ayuda para ver la lista.");
  }
}

// ---------------- Tarea de Telegram (núcleo 0) ----------------
const char COMANDOS_JSON[] = R"json({"commands":[
{"command":"estado","description":"Ver cómo están las macetas"},
{"command":"regar","description":"Regar: /regar 1, /regar kiwi o /regar todas"},
{"command":"detener","description":"Cortar el riego en curso"},
{"command":"pausa","description":"Pausar el riego automático"},
{"command":"reanudar","description":"Reanudar el riego automático"},
{"command":"horarios","description":"Ver los horarios de riego"},
{"command":"ayuda","description":"Ver la lista de comandos"}]})json";

void setEstadoTg(const char* s) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  strlcpy(tgEstado, s, sizeof(tgEstado));
  xSemaphoreGive(mtx);
}

int tgPost(const char* token, const char* metodo, const String& cuerpo, String* respuesta,
           const char* tipo = "application/x-www-form-urlencoded") {
  WiFiClientSecure cliente;
  cliente.setInsecure();                   // sin validar certificado (más simple)
  HTTPClient http;
  http.setTimeout(15000);
  String url = "https://api.telegram.org/bot";
  url += token;
  url += "/";
  url += metodo;
  if (!http.begin(cliente, url)) return -1;
  http.addHeader("Content-Type", tipo);
  int codigo = http.POST(cuerpo);
  if (respuesta && codigo > 0) *respuesta = http.getString();
  http.end();
  return codigo;
}

void tareaTelegram(void*) {
  int64_t offset = 0;
  bool descartarViejos = true;             // no ejecutar comandos viejos al arrancar
  char token[64], chat[24];
  MsgTg m;

  for (;;) {
    bool activo, comandos;
    xSemaphoreTake(mtx, portMAX_DELAY);
    activo = tg.activo;
    comandos = tg.comandos;
    strlcpy(token, tg.token.c_str(), sizeof(token));
    strlcpy(chat, tg.chatId.c_str(), sizeof(chat));
    xSemaphoreGive(mtx);

    if (!activo || token[0] == 0) {
      setEstadoTg(activo ? "Falta el token del bot" : "Desactivado");
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {}
      descartarViejos = true;
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      setEstadoTg("Sin conexión WiFi");
      vTaskDelay(pdMS_TO_TICKS(3000));
      continue;
    }

    if (tgReconfigurar) {
      tgReconfigurar = false;
      descartarViejos = true;
      int c = tgPost(token, "setMyCommands", COMANDOS_JSON, nullptr, "application/json");
      if (c != 200) {
        char b[100];
        if (c == 401 || c == 404) snprintf(b, sizeof(b), "Token inválido. Copialo de nuevo desde @BotFather");
        else snprintf(b, sizeof(b), "No se pudo conectar con Telegram (código %d)", c);
        setEstadoTg(b);
        tgReconfigurar = true;
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
      setEstadoTg(chat[0] ? "Conectado" : "Conectado. Falta autorizar tu chat");
    }

    // Enviar los mensajes pendientes
    if (chat[0]) {
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {
        String cuerpo = "chat_id=";
        cuerpo += chat;
        cuerpo += "&text=";
        cuerpo += urlEncode(m.texto);
        int c = tgPost(token, "sendMessage", cuerpo, nullptr);
        if (c == 200) {
          setEstadoTg("Conectado");
        } else {
          char b[100];
          if (c == 400 || c == 403) snprintf(b, sizeof(b), "No se pudo enviar. Revisá el ID del chat y que le hayas escrito al bot (código %d)", c);
          else snprintf(b, sizeof(b), "No se pudo enviar (código %d)", c);
          setEstadoTg(b);
        }
      }
    } else {
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {}
    }

    // Recibir mensajes (también sirve para detectar el ID de tu chat)
    if (!comandos && chat[0]) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    char cuerpoG[64];
    if (descartarViejos) snprintf(cuerpoG, sizeof(cuerpoG), "offset=-1&timeout=0");
    else snprintf(cuerpoG, sizeof(cuerpoG), "offset=%lld&timeout=8", (long long)offset);
    String resp;
    int c = tgPost(token, "getUpdates", cuerpoG, &resp);
    if (c != 200) {
      if (c == 401 || c == 404) setEstadoTg("Token inválido. Copialo de nuevo desde @BotFather");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    JsonDocument filtro;
    filtro["result"][0]["update_id"] = true;
    filtro["result"][0]["message"]["chat"]["id"] = true;
    filtro["result"][0]["message"]["text"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, resp, DeserializationOption::Filter(filtro))) continue;

    for (JsonObject u : doc["result"].as<JsonArray>()) {
      offset = u["update_id"].as<int64_t>() + 1;
      if (descartarViejos) continue;
      int64_t cid = u["message"]["chat"]["id"].as<int64_t>();
      if (cid == 0) continue;
      const char* texto = u["message"]["text"] | "";
      char cidTexto[24];
      snprintf(cidTexto, sizeof(cidTexto), "%lld", (long long)cid);

      if (strcmp(cidTexto, chat) != 0) {
        // Chat sin autorizar: se informa su ID en la web y se le responde
        xSemaphoreTake(mtx, portMAX_DELAY);
        strlcpy(tgDesconocido, cidTexto, sizeof(tgDesconocido));
        xSemaphoreGive(mtx);
        String aviso = String("Este bot de riego todavía no te tiene autorizado. Tu ID de chat es ") + cidTexto +
                       ". Cargalo en la pestaña Notificaciones de la web del riego.";
        String cuerpo = String("chat_id=") + cidTexto + "&text=" + urlEncode(aviso.c_str());
        tgPost(token, "sendMessage", cuerpo, nullptr);
        continue;
      }
      if (!comandos) continue;
      CmdTg cmd;
      strlcpy(cmd.texto, texto, sizeof(cmd.texto));
      xQueueSend(colaEntrada, &cmd, 0);
    }
    descartarViejos = false;
  }
}

// ---------------- Página web ----------------
const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Riego</title>
<style>
:root{color-scheme:light;--fondo:#eef3ef;--hoja:#2f5d3a;--hortensia:#4a6fa5;--hortensia-claro:#dfe7f3;--tierra:#8a6a4f;--tarjeta:#fff;--texto:#1d2a20;--suave:#5d6b61;--linea:#cfd9d1;--alerta:#a8442a}
@media(prefers-color-scheme:dark){:root:not([data-tema="claro"]){--fondo:#141a16;--hoja:#7fbf8e;--hortensia:#8fb0e0;--hortensia-claro:#24304a;--tierra:#c49a76;--tarjeta:#1d2520;--texto:#e6ece7;--suave:#9fb0a4;--linea:#33413a;--alerta:#e58a6c;color-scheme:dark}}
:root[data-tema="oscuro"]{--fondo:#141a16;--hoja:#7fbf8e;--hortensia:#8fb0e0;--hortensia-claro:#24304a;--tierra:#c49a76;--tarjeta:#1d2520;--texto:#e6ece7;--suave:#9fb0a4;--linea:#33413a;--alerta:#e58a6c;color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--fondo);color:var(--texto);line-height:1.45}
main{max-width:620px;margin:auto;padding:16px 14px 40px}
h1{font-size:26px;margin:4px 0 0;color:var(--hoja);letter-spacing:-.5px}
h2{font-size:17px;margin:26px 0 10px;color:var(--hoja)}
.suave{color:var(--suave);font-size:14px}
.maceta{background:var(--tarjeta);border:1px solid var(--linea);border-left:6px solid var(--hortensia);border-radius:0 12px 12px 0;padding:12px 14px;margin-bottom:10px}
.maceta.activa{border-left-color:var(--hoja)}
.nombre{font-size:18px;font-weight:600}
.humedad{position:relative;height:14px;background:var(--hortensia-claro);border-radius:7px;margin:8px 0 4px;overflow:hidden}
.humedad .agua{height:100%;background:var(--hortensia)}
.humedad .umbral{position:absolute;top:0;bottom:0;width:2px;background:var(--tierra)}
.panel{background:var(--tarjeta);border:1px solid var(--linea);border-radius:12px;padding:10px 14px;margin-bottom:10px}
.aviso{background:var(--hortensia-claro);color:var(--hortensia);border-radius:12px;padding:10px 14px;margin-bottom:10px;font-size:14px}
.campo{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid var(--linea);font-size:15px}
.campo:last-child{border-bottom:0}
.campo.vertical{flex-direction:column;align-items:stretch;gap:6px}
input{font:inherit;padding:6px 8px;border:1px solid var(--linea);border-radius:6px;background:var(--fondo);color:var(--texto);width:100px}
input[type=checkbox]{width:22px;height:22px;flex:0 0 auto;accent-color:var(--hoja)}
input.ancho{width:150px}
input.largo{width:100%}
button{font:inherit;font-weight:600;border:0;border-radius:8px;padding:10px 16px;background:var(--hoja);color:var(--tarjeta);cursor:pointer}
button.chico{padding:6px 10px;font-size:13px;background:var(--hortensia-claro);color:var(--hortensia)}
button.detener{background:var(--alerta)}
button:disabled{opacity:.45;cursor:default}
button:focus-visible,input:focus-visible{outline:3px solid var(--hortensia);outline-offset:2px}
.fila{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.msg{margin-top:10px;font-size:14px;min-height:1.2em}
.msg.error{color:var(--alerta)}
.pestanas{display:flex;flex-wrap:wrap;gap:6px;margin:14px 0 0}
.pestanas button{background:transparent;color:var(--suave);border:1px solid var(--linea);border-radius:20px;padding:7px 14px}
.pestanas button.sel{background:var(--hoja);color:var(--tarjeta);border-color:var(--hoja)}
.tab{display:none}
.tab.sel{display:block}
</style>
</head>
<body><main>
<div class="fila" style="justify-content:space-between"><h1>Riego</h1><button class="chico" id="tema" onclick="cambiarTema()">Tema: automático</button></div>
<div class="suave" id="hora">Conectando con el ESP32…</div>
<nav class="pestanas" id="pestanas"><button data-t="estado" class="sel" onclick="mostrar('estado')">Estado</button><button data-t="general" onclick="mostrar('general')">Horarios</button><button data-t="telegram" onclick="mostrar('telegram')">Notificaciones</button></nav>

<section class="tab sel" id="t-estado">
<h2>Macetas</h2>
<div id="pausaBox"></div>
<div id="estado"></div>
<div class="fila"><button class="detener" onclick="detener()">Detener riego</button><button class="chico" id="btnPausa" onclick="pausar()">Pausar riego automático</button></div>
</section>

<section class="tab" id="t-general">
<h2>Horarios</h2>
<div class="panel"><div class="suave" style="padding:4px 0 6px">Se usan para las macetas que no tienen el sensor activado.</div><div id="horarios"></div></div>
<h2>Seguridad</h2>
<div class="panel"><div class="campo"><span>Tiempo máximo de un riego (s)</span><input type="number" id="maxs" min="1" max="300"></div></div>
</section>

<section class="tab" id="t-telegram">
<h2>Notificaciones y comandos</h2>
<div class="aviso">Para crear tu bot, abrí Telegram, buscá @BotFather y mandale /newbot. Al terminar te da un token: pegalo acá, activá Telegram y guardá. Después escribile cualquier mensaje a tu bot y acá va a aparecer tu ID de chat para autorizarlo.</div>
<div class="panel">
<div class="campo"><label class="fila" for="tgOn">Activar Telegram</label><input type="checkbox" id="tgOn"></div>
<div class="campo vertical"><label for="tgTok">Token del bot</label><input class="largo" type="password" id="tgTok" autocomplete="off" placeholder="123456789:AAH4k2xYz..."></div>
<div class="campo vertical"><label for="tgChat">ID del chat autorizado</label><input class="largo" id="tgChat" inputmode="numeric" placeholder="123456789"></div>
<div class="campo"><span>Conexión</span><span id="tgEstado" class="suave" style="text-align:right">…</span></div>
<div id="tgDesc"></div>
</div>
<h2>Avisos</h2>
<div class="panel">
<div class="campo"><label for="nRie">Cada vez que se riega una maceta</label><input type="checkbox" id="nRie"></div>
<div class="campo"><label for="nMax">Si una maceta con sensor llega al máximo de riegos del día</label><input type="checkbox" id="nMax"></div>
<div class="campo"><label for="nIni">Cuando se enciende el sistema, por ejemplo después de un corte de luz</label><input type="checkbox" id="nIni"></div>
<div class="campo"><label for="nRes">Resumen diario a las</label><span class="fila" style="flex-wrap:nowrap"><input type="time" id="rHora"><input type="checkbox" id="nRes"></span></div>
</div>
<h2>Comandos</h2>
<div class="panel">
<div class="campo"><label for="tgCmd">Aceptar comandos desde Telegram</label><input type="checkbox" id="tgCmd"></div>
<div class="suave" style="padding:8px 0">/estado muestra cómo están las macetas. /regar 1, /regar kiwi o /regar todas riegan con la cantidad configurada. /detener corta el riego en curso. /pausa y /reanudar frenan o retoman el riego automático. /horarios lista los horarios y /ayuda muestra los comandos. Solo responde a tu chat autorizado.</div>
</div>
<div class="fila"><button class="chico" onclick="probarTg()">Enviar mensaje de prueba</button></div>
<div class="msg" id="msgTg"></div>
</section>

<div id="config"></div>

<div id="guardarBox" style="display:none;margin-top:16px">
<button onclick="guardar()">Guardar ajustes</button>
<div class="suave" style="margin-top:6px">Guarda los cambios de todas las pestañas.</div>
<div class="msg" id="msg"></div>
</div>
</main>
<script>
let est=null,cfg=null,pest='estado';
const TEMAS=['auto','claro','oscuro'],NOMBRE_TEMA={auto:'automático',claro:'claro',oscuro:'oscuro'};
let tema='auto';
try{tema=localStorage.getItem('tema')||'auto';}catch(e){}
function aplicarTema(){
  if(tema==='auto')document.documentElement.removeAttribute('data-tema');
  else document.documentElement.setAttribute('data-tema',tema);
  document.getElementById('tema').textContent='Tema: '+NOMBRE_TEMA[tema];
}
function cambiarTema(){tema=TEMAS[(TEMAS.indexOf(tema)+1)%3];try{localStorage.setItem('tema',tema);}catch(e){}aplicarTema();}
aplicarTema();

const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dos=n=>String(n).padStart(2,'0');

function mostrar(id){
  pest=id;
  document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('sel',t.id==='t-'+id));
  document.querySelectorAll('#pestanas button').forEach(b=>b.classList.toggle('sel',b.dataset.t===id));
  $('guardarBox').style.display=id==='estado'?'none':'block';
}
function pintarPestanas(){
  const lista=[['estado','Estado'],['general','Horarios'],['telegram','Notificaciones']].concat(cfg.macetas.map((m,i)=>['m'+i,m.nombre]));
  $('pestanas').innerHTML=lista.map(([id,n])=>`<button data-t="${id}" onclick="mostrar('${id}')">${esc(n)}</button>`).join('');
  mostrar(pest);
}
function irA(el){const t=el.closest('.tab');if(t)mostrar(t.id.slice(2));el.focus();}

async function cargarEstado(){
  try{const r=await fetch('/api/estado');est=await r.json();pintarEstado();}
  catch(e){$('hora').textContent='Sin conexión con el ESP32. Revisá que estés en la misma red WiFi.';}
}
function pintarEstado(){
  $('hora').textContent='Hora del sistema: '+est.hora+'. Señal WiFi: '+est.rssi+' dBm';
  $('pausaBox').innerHTML=est.pausa?'<div class="aviso">El riego automático está en pausa. Los riegos manuales siguen funcionando.</div>':'';
  $('btnPausa').textContent=est.pausa?'Reanudar riego automático':'Pausar riego automático';
  $('tgEstado').textContent=est.tgEstado;
  $('tgDesc').innerHTML=est.tgDesconocido
    ?`<div class="campo"><span class="suave">Te escribió un chat sin autorizar, con el ID ${esc(est.tgDesconocido)}</span><button class="chico" onclick="autorizar('${esc(est.tgDesconocido)}')">Autorizar</button></div>`:'';
  $('estado').innerHTML=est.macetas.map((m,i)=>{
    const enCurso=est.regando===i;
    const umbral=cfg?cfg.macetas[i].umbral:0;
    const hum=m.sensor
      ?`<div class="humedad"><div class="agua" style="width:${m.pct}%"></div><div class="umbral" style="left:${umbral}%"></div></div><div class="suave">Humedad ${m.pct} %, riega por debajo de ${umbral} % (lectura ${m.raw})</div>`
      :`<div class="suave">Riego por horario. Lectura del sensor: ${m.raw}</div>`;
    const ahora=enCurso?`Regando, faltan ${est.restante} s`:(m.pendiente?'En espera para regar':`Último riego: ${esc(m.ultimo)}`);
    return `<div class="maceta${enCurso?' activa':''}"><div class="nombre">${esc(m.nombre)}</div>${hum}
<div class="suave">Hoy: ${m.mlHoy} ml en total, ${m.riegosHoy} riegos automáticos</div><div class="suave">${ahora}</div>
<div class="fila" style="margin-top:10px"><button ${enCurso||m.pendiente?'disabled':''} onclick="regar(${i})">Regar ${m.ml} ml</button></div></div>`;
  }).join('');
}
async function regar(i){await fetch('/api/regar',{method:'POST',body:new URLSearchParams({m:i})});cargarEstado();}
async function detener(){await fetch('/api/detener',{method:'POST'});cargarEstado();}
async function pausar(){await fetch('/api/pausa',{method:'POST',body:new URLSearchParams({on:est&&est.pausa?0:1})});cargarEstado();}
function autorizar(id){$('tgChat').value=id;aviso('ID cargado. Guardá los ajustes para autorizarlo.',false);}

async function probarTg(){
  const el=$('msgTg');el.className='msg';el.textContent='Enviando…';
  try{const r=await fetch('/api/telegram/prueba',{method:'POST'});el.textContent=await r.text();el.className='msg'+(r.ok?'':' error');}
  catch(e){el.textContent='No se pudo conectar con el ESP32.';el.className='msg error';}
}

async function cargarConfig(){
  const r=await fetch('/api/config');cfg=await r.json();
  $('maxs').value=cfg.maxs;
  $('horarios').innerHTML=cfg.horarios.map((h,j)=>`<div class="campo"><label class="fila"><input type="checkbox" id="ha${j}" ${h.a?'checked':''}>Horario ${j+1}</label><input type="time" id="ht${j}" value="${dos(h.h)}:${dos(h.m)}"></div>`).join('');
  const t=cfg.tg;
  $('tgOn').checked=t.on;$('tgTok').value='';
  $('tgTok').placeholder=t.tokenGuardado?'Token guardado. Dejalo vacío para no cambiarlo.':'123456789:AAH4k2xYz...';
  $('tgChat').value=t.chat;$('tgCmd').checked=t.cmd;$('nRie').checked=t.riego;$('nMax').checked=t.max;
  $('nIni').checked=t.ini;$('nRes').checked=t.res;$('rHora').value=dos(t.rh)+':'+dos(t.rm);
  $('config').innerHTML=cfg.macetas.map((m,i)=>`<section class="tab" id="t-m${i}"><h2>Ajustes de ${esc(m.nombre)}</h2><div class="panel">
<div class="campo"><span>Nombre</span><input class="ancho" id="n${i}" maxlength="20" value="${esc(m.nombre)}"></div>
<div class="campo"><span>Agua por riego (ml)</span><input type="number" id="ml${i}" min="10" max="3000" value="${m.ml}"></div>
<div class="campo"><span>Caudal medido de la bomba (ml/s)</span><input type="number" step="0.1" id="q${i}" min="1" max="200" value="${m.caudal}"></div>
<div class="campo"><label class="fila" for="s${i}">Regar según el sensor</label><input type="checkbox" id="s${i}" ${m.sensor?'checked':''}></div>
<div class="campo"><span>Regar si la humedad baja de (%)</span><input type="number" id="u${i}" min="0" max="100" value="${m.umbral}"></div>
<div class="campo"><span>Lectura con tierra seca</span><span class="fila"><input type="number" id="as${i}" min="0" max="4095" value="${m.seco}"><button class="chico" onclick="tomar('as',${i})">Usar actual</button></span></div>
<div class="campo"><span>Lectura con tierra mojada</span><span class="fila"><input type="number" id="am${i}" min="0" max="4095" value="${m.mojado}"><button class="chico" onclick="tomar('am',${i})">Usar actual</button></span></div>
<div class="campo"><span>Espera mínima entre riegos (min)</span><input type="number" id="e${i}" min="0" max="1440" value="${m.espera}"></div>
<div class="campo"><span>Máximo de riegos automáticos por día</span><input type="number" id="x${i}" min="0" max="24" value="${m.maxDia}"></div>
</div></section>`).join('');
  pintarPestanas();
  if(est)pintarEstado();
}
function tomar(p,i){if(est){$(p+i).value=est.macetas[i].raw;limpiar();}}
function limpiar(){$('msg').textContent='';$('msg').className='msg';}
function aviso(t,error){$('msg').textContent=t;$('msg').className='msg'+(error?' error':'');}

async function guardar(){
  const p=new URLSearchParams();let err='';
  const num=(id,min,max,nombre)=>{const el=$(id),v=Number(el.value);
    if(!err&&(el.value===''||isNaN(v)||v<min||v>max)){err=`Revisá "${nombre}": tiene que estar entre ${min} y ${max}.`;irA(el);}
    return v;};
  p.set('maxs',num('maxs',1,300,'Tiempo máximo de un riego'));
  cfg.horarios.forEach((h,j)=>{const t=$('ht'+j).value,activo=$('ha'+j).checked;
    if(!err&&activo&&!t){err=`Completá la hora del horario ${j+1}.`;irA($('ht'+j));}
    const [hh,mm]=(t||'00:00').split(':');p.set('ha'+j,activo?1:0);p.set('hh'+j,Number(hh));p.set('hm'+j,Number(mm));});

  const on=$('tgOn').checked,tok=$('tgTok').value.trim(),chat=$('tgChat').value.trim(),rh=$('rHora').value;
  if(!err&&on&&!tok&&!cfg.tg.tokenGuardado){err='Para activar Telegram, pegá el token de tu bot.';irA($('tgTok'));}
  if(!err&&tok&&!/^\d+:[\w-]{20,}$/.test(tok)){err='El token no tiene el formato esperado. Copialo completo desde @BotFather.';irA($('tgTok'));}
  if(!err&&chat&&!/^-?\d{3,20}$/.test(chat)){err='El ID del chat tiene que ser un número, por ejemplo 123456789.';irA($('tgChat'));}
  if(!err&&$('nRes').checked&&!rh){err='Elegí la hora del resumen diario.';irA($('rHora'));}
  const [rH,rM]=(rh||'21:00').split(':');
  p.set('tgOn',on?1:0);if(tok)p.set('tgTok',tok);p.set('tgChat',chat);
  p.set('tgCmd',$('tgCmd').checked?1:0);p.set('nRie',$('nRie').checked?1:0);p.set('nMax',$('nMax').checked?1:0);
  p.set('nIni',$('nIni').checked?1:0);p.set('nRes',$('nRes').checked?1:0);p.set('rH',Number(rH));p.set('rM',Number(rM));

  cfg.macetas.forEach((m,i)=>{const n=$('n'+i).value.trim();
    if(!err&&!n){err=`La maceta ${i+1} necesita un nombre.`;irA($('n'+i));}
    p.set('n'+i,n);
    p.set('ml'+i,num('ml'+i,10,3000,'Agua por riego'));
    p.set('q'+i,num('q'+i,1,200,'Caudal medido'));
    p.set('s'+i,$('s'+i).checked?1:0);
    p.set('u'+i,num('u'+i,0,100,'Humedad mínima'));
    const s=num('as'+i,0,4095,'Lectura con tierra seca'),w=num('am'+i,0,4095,'Lectura con tierra mojada');
    if(!err&&s===w){err=`En ${n}, las lecturas con tierra seca y mojada tienen que ser distintas.`;irA($('am'+i));}
    p.set('as'+i,s);p.set('am'+i,w);
    p.set('e'+i,num('e'+i,0,1440,'Espera mínima'));
    p.set('x'+i,num('x'+i,0,24,'Máximo de riegos por día'));});
  if(err){aviso(err,true);return;}
  try{const r=await fetch('/api/config',{method:'POST',body:p});
    if(r.ok){aviso('Ajustes guardados.',false);cargarConfig();}else aviso('El ESP32 rechazó los ajustes. Volvé a intentarlo.',true);}
  catch(e){aviso('No se pudo conectar con el ESP32. Los ajustes no se guardaron.',true);}
}
document.addEventListener('input',limpiar);
cargarConfig().then(cargarEstado);
setInterval(cargarEstado,3000);
</script>
</body></html>
)rawliteral";

// ---------------- API ----------------
int argInt(const String& clave, int def, int mn, int mx) {
  if (!server.hasArg(clave)) return def;
  return constrain(server.arg(clave).toInt(), mn, mx);
}

float argFloat(const String& clave, float def, float mn, float mx) {
  if (!server.hasArg(clave)) return def;
  return constrain(server.arg(clave).toFloat(), mn, mx);
}

void handleEstado() {
  long restante = 0;
  if (regando >= 0) restante = ((long)duracionRiego - (long)(millis() - inicioRiego)) / 1000 + 1;
  if (restante < 0) restante = 0;
  char estadoTg[100], desconocido[24];
  xSemaphoreTake(mtx, portMAX_DELAY);
  strlcpy(estadoTg, tgEstado, sizeof(estadoTg));
  strlcpy(desconocido, tgDesconocido, sizeof(desconocido));
  xSemaphoreGive(mtx);

  String j = "{\"hora\":\"";
  j += horaTexto();
  j += "\",\"rssi\":";
  j += (int)WiFi.RSSI();
  j += ",\"regando\":";
  j += regando;
  j += ",\"restante\":";
  j += restante;
  j += ",\"pausa\":";
  j += pausa ? "true" : "false";
  j += ",\"tgEstado\":\"";
  j += jsonEsc(estadoTg);
  j += "\",\"tgDesconocido\":\"";
  j += desconocido;
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

void handleConfigGet() {
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

void handleConfigPost() {
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
      if (n.length() > 0) mac[i].nombre = n.substring(0, 40);
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

  xSemaphoreTake(mtx, portMAX_DELAY);
  tg.activo = argInt("tgOn", tg.activo, 0, 1);
  if (server.hasArg("tgTok")) {
    String t = server.arg("tgTok");
    t.trim();
    if (t.length() > 0 && t.length() < 64) tg.token = t;
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
  xSemaphoreGive(mtx);
  tgReconfigurar = true;

  guardarConfig();
  Serial.println("Ajustes guardados desde la web");
  server.send(200, "text/plain", "ok");
}

void handleRegar() {
  int m = argInt("m", -1, -1, N - 1);
  if (m >= 0) encolar(m, 'w');
  server.send(200, "text/plain", "ok");
}

void handleDetener() {
  detenerTodo();
  server.send(200, "text/plain", "ok");
}

void handlePausa() {
  pausa = argInt("on", pausa, 0, 1);
  guardarPausa();
  server.send(200, "text/plain", "ok");
}

void handlePrueba() {
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

// ---------------- Setup y loop ----------------
void setup() {
  for (int i = 0; i < N; i++) {
    pinMode(PIN_BOMBA[i], OUTPUT);
    digitalWrite(PIN_BOMBA[i], LOW);
  }
  pinMode(BOTON, INPUT_PULLUP);
  analogReadResolution(12);                 // lecturas de 0 a 4095

  Serial.begin(115200);
  delay(500);
  Serial.println("\nRiego con web iniciando...");

  mtx = xSemaphoreCreateMutex();
  colaSalida = xQueueCreate(6, sizeof(MsgTg));
  colaEntrada = xQueueCreate(4, sizeof(CmdTg));
  cargarConfig();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" conectado. Web en http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" sin conexión (sigue intentando)");
  }
  if (MDNS.begin(NOMBRE_MDNS)) Serial.printf("También en http://%s.local\n", NOMBRE_MDNS);

  configTzTime(ZONA_HORARIA, "pool.ntp.org", "time.google.com");

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGINA); });
  server.on("/api/estado", HTTP_GET, handleEstado);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/regar", HTTP_POST, handleRegar);
  server.on("/api/detener", HTTP_POST, handleDetener);
  server.on("/api/pausa", HTTP_POST, handlePausa);
  server.on("/api/telegram/prueba", HTTP_POST, handlePrueba);
  server.begin();

  // Telegram corre en el núcleo 0 para no demorar el riego ni la web
  xTaskCreatePinnedToCore(tareaTelegram, "telegram", 12288, nullptr, 1, nullptr, 0);
  if (tg.avisarInicio) notificar("🔌 El sistema de riego se encendió. Si no lo reiniciaste vos, puede haber habido un corte de luz.");

  leerSensores();
}

void loop() {
  server.handleClient();
  gestionarRiego();

  // Comandos recibidos por Telegram
  CmdTg c;
  while (xQueueReceive(colaEntrada, &c, 0) == pdTRUE) procesarComando(String(c.texto));

  static unsigned long ultimaLectura = 0, ultimaLogica = 0, ultimaIp = 0;
  if (millis() - ultimaLectura > 2000) { ultimaLectura = millis(); leerSensores(); }
  if (millis() - ultimaLogica > 10000) { ultimaLogica = millis(); logicaAutomatica(); }
  if (millis() - ultimaIp > 300000) {
    ultimaIp = millis();
    Serial.printf("%s | IP %s\n", horaTexto().c_str(), WiFi.localIP().toString().c_str());
  }

  // Botón BOOT: riega todas
  if (digitalRead(BOTON) == LOW) {
    delay(50);
    if (digitalRead(BOTON) == LOW) {
      Serial.println("Riego manual (botón)");
      for (int i = 0; i < N; i++) encolar(i, 'b');
      while (digitalRead(BOTON) == LOW) delay(10);
    }
  }
}
