#include "utilidades.h"
#include <time.h>

String horaTexto() {
  struct tm t;
  if (!getLocalTime(&t, 10)) return "sin hora";
  char b[32];
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
