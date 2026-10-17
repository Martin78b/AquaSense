// Funciones de texto compartidas.
#pragma once

#include <Arduino.h>

String horaTexto();
String dosDigitos(int n);
String jsonEsc(const String& s);
String urlEncode(const char* s);
