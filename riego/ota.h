// Actualizaciones inalámbricas (OTA): local vía Web, remota vía GitHub Releases y Telegram.
#pragma once

#include <Arduino.h>
#include <WebServer.h>

void iniciarOTA(WebServer& server);
bool otaVerificarGitHub(String& nuevaVersion, String& urlDescarga, String& notas, String& err);
bool otaActualizarDesdeUrl(const String& url, String& err);
