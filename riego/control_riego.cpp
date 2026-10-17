#include "control_riego.h"
#include "config.h"
#include "utilidades.h"
#include "comandos.h"
#include "telegram.h"
#include <esp_timer.h>
#include <time.h>

int regando = -1;

static unsigned long inicioRiego = 0, duracionRiego = 0, finUltimoRiego = 0;
static int diaActual = -1;
static int diaResumen = -1;
static unsigned long ultimoResetSinHora = 0;
static esp_timer_handle_t corteSeguridad = nullptr;

static const char* origenTexto(char o) {
  switch (o) {
    case 'h': return "por horario";
    case 's': return "por sensor";
    case 't': return "desde Telegram";
    case 'b': return "con el botón";
    default:  return "desde la web";
  }
}

static void apagarBombas() {
  for (int i = 0; i < N; i++) digitalWrite(PIN_BOMBA[i], LOW);
}

// Corta la bomba al cumplirse la duración aunque el loop esté demorado
// (por ejemplo, atendiendo un cliente web lento).
static void cortarPorTiempo(void*) { apagarBombas(); }

void iniciarBombas() {
  for (int i = 0; i < N; i++) {
    pinMode(PIN_BOMBA[i], OUTPUT);
    digitalWrite(PIN_BOMBA[i], LOW);
  }
  pinMode(BOTON, INPUT_PULLUP);
  esp_timer_create_args_t args = {};
  args.callback = cortarPorTiempo;
  args.name = "corte_bomba";
  esp_timer_create(&args, &corteSeguridad);
}

void detenerTodo() {
  esp_timer_stop(corteSeguridad);
  apagarBombas();
  if (regando >= 0) Serial.printf("Riego de %s detenido\n", mac[regando].nombre.c_str());
  regando = -1;
  finUltimoRiego = millis();
  for (int i = 0; i < N; i++) mac[i].pendiente = false;
}

static void iniciarRiego(int i) {
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
  esp_timer_stop(corteSeguridad);
  esp_timer_start_once(corteSeguridad, (uint64_t)duracionRiego * 1000);
  digitalWrite(PIN_BOMBA[i], HIGH);
}

void gestionarRiego() {
  if (regando >= 0) {
    if (millis() - inicioRiego >= duracionRiego) {
      int i = regando;
      esp_timer_stop(corteSeguridad);
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

long segundosRestantes() {
  if (regando < 0) return 0;
  long restante = ((long)duracionRiego - (long)(millis() - inicioRiego)) / 1000 + 1;
  return restante < 0 ? 0 : restante;
}

bool encolar(int i, char origen) {
  if (mac[i].pendiente || regando == i) return false;
  mac[i].pendiente = true;
  mac[i].origen = origen;
  return true;
}

static void encolarAutomatico(int i, char origen) {
  if (mac[i].pendiente || regando == i) return;
  if (mac[i].riegosHoy >= mac[i].maxDia) return;
  mac[i].riegosHoy++;
  encolar(i, origen);
  if (mac[i].usarSensor && mac[i].riegosHoy >= mac[i].maxDia && tg.avisarMaximo) {
    notificar(String("⚠️ ") + mac[i].nombre + " llegó al máximo de " + mac[i].maxDia +
              " riegos automáticos de hoy. Si la tierra sigue seca, revisá el sensor y la manguera.");
  }
}

static void reiniciarContadores() {
  for (int i = 0; i < N; i++) { mac[i].riegosHoy = 0; mac[i].mlHoy = 0; }
}

// Botón BOOT sin bloquear el loop: antirrebote de 50 ms y una acción por pulsación
void revisarBoton() {
  static int ultimaLectura = HIGH, estable = HIGH;
  static unsigned long ultimoCambio = 0;
  int lectura = digitalRead(BOTON);
  if (lectura != ultimaLectura) {
    ultimaLectura = lectura;
    ultimoCambio = millis();
  }
  if (lectura != estable && millis() - ultimoCambio >= 50) {
    estable = lectura;
    if (estable == LOW) {
      Serial.println("Riego manual (botón)");
      for (int i = 0; i < N; i++) encolar(i, 'b');
    }
  }
}

void logicaAutomatica() {
  struct tm t;
  bool hayHora = getLocalTime(&t, 10);

  // Contadores diarios. La primera vez que llega la hora solo se toma el día,
  // para no borrar los riegos que se hayan hecho antes de sincronizar.
  if (hayHora && t.tm_yday != diaActual) {
    if (diaActual >= 0) reiniciarContadores();
    diaActual = t.tm_yday;
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

  // Sensores: riega si está seca y ya pasó la espera mínima.
  // Las macetas con el sensor desactivado nunca entran acá.
  if (pausa) return;
  for (int i = 0; i < N; i++) {
    if (!mac[i].usarSensor) continue;
    bool esperaCumplida = !mac[i].yaRego || millis() - mac[i].ultimoRiegoMs >= mac[i].esperaMin * 60000UL;
    if (mac[i].pct < mac[i].umbral && esperaCumplida) encolarAutomatico(i, 's');
  }
}
