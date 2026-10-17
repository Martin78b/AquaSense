#include "config.h"
#include <Preferences.h>

Maceta mac[N];
Horario hor[MAX_HORARIOS];
ConfigTelegram tg;
int maxSegundos = 60;
bool pausa = false;

static Preferences prefs;

static String k(const char* a, int i) { return String(a) + i; }

static void valoresPorDefecto() {
  const char* nombres[N] = {"Hortensia A", "Hortensia B", "Kiwi C"};
  const int ml[N]     = {400, 400, 150};
  const int umbral[N] = {55, 55, 38};
  const int espera[N] = {120, 120, 60};
  const int maxDia[N] = {3, 3, 2};
  for (int i = 0; i < N; i++) {
    mac[i].nombre = nombres[i];
    mac[i].ml = ml[i];
    mac[i].caudal = 20.0;
    mac[i].usarSensor = false;               // vienen desactivados
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
