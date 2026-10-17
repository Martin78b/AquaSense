#include "comandos.h"
#include "config.h"
#include "control_riego.h"
#include "telegram.h"
#include "utilidades.h"

static String textoAyuda() {
  return String("🌱 Comandos del riego\n\n") +
         "/estado: cómo están las macetas\n" +
         "/regar 1, /regar kiwi o /regar todas: riega con la cantidad configurada\n" +
         "/detener: corta el riego en curso\n" +
         "/pausa: frena el riego automático (los manuales siguen andando)\n" +
         "/reanudar: retoma el riego automático\n" +
         "/horarios: muestra los horarios de riego\n" +
         "/ayuda: muestra esta lista";
}

static String textoEstado() {
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

static String textoHorarios() {
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
