// Bot de Telegram en una tarea propia (núcleo 0), comunicada por colas.
#pragma once

#include <Arduino.h>

void iniciarColasTelegram();               // antes de cargar la configuración
void iniciarTareaTelegram();               // después de conectar el WiFi
void notificar(const String& texto);       // pone el mensaje en la cola, no bloquea
bool recibirComando(String& texto);        // comando del chat autorizado, si hay

// Mutex que protege `tg`, tgReconfigurar, tgEstado y tgDesconocido
void tgBloquear();
void tgLiberar();
extern bool tgReconfigurar;                // descarta mensajes viejos y registra los comandos
extern char tgEstado[100];
extern char tgDesconocido[24];             // último chat sin autorizar que escribió
