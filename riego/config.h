// Datos fijos, estado compartido y persistencia de la configuración.
#pragma once

#include <Arduino.h>

// ===================== DATOS FIJOS =====================
#define AQUASENSE_VERSION "1.0.0"
#define GITHUB_REPO       "Martin78b/AquaSense"
#define ZONA_HORARIA      "<-03>3"              // Argentina (UTC-3)
#define NOMBRE_MDNS       "riego"               // http://riego.local
#define NTP_1        "pool.ntp.org"
#define NTP_2        "time.google.com"

const int N = 3;
const int PIN_BOMBA[N]  = {25, 26, 27};    // IN1, IN2, IN3 del ULN2003
const int PIN_SENSOR[N] = {32, 33, 34};    // sensores de humedad (ADC1)
const int BOTON = 0;                       // botón BOOT: riega todas
const int MAX_HORARIOS = 4;
const int LARGO_NOMBRE = 20;
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

extern Maceta mac[N];
extern Horario hor[MAX_HORARIOS];
extern ConfigTelegram tg;   // escrita solo desde el programa principal, con el mutex de telegram.h
extern int maxSegundos;
extern bool pausa;

void cargarConfig();
void guardarConfig();
void guardarPausa();
