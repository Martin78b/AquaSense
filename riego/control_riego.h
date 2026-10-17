// Cola de riego, bombas y lógica automática (horarios y sensores).
#pragma once

#include <Arduino.h>

// Origen de cada riego: 'h' horario, 's' sensor, 'w' web, 't' Telegram, 'b' botón
extern int regando;                        // índice de la maceta regando, o -1

void iniciarBombas();                      // primero en setup(): todas en LOW
void gestionarRiego();
void revisarBoton();
void logicaAutomatica();
bool encolar(int i, char origen);
void detenerTodo();
long segundosRestantes();
