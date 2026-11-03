#include "ota.h"
#include "config.h"
#include "control_riego.h"
#include "utilidades.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

static String getUpdateErrorString() {
  uint8_t err = Update.getError();
  switch (err) {
    case UPDATE_ERROR_OK: return "OK";
    case UPDATE_ERROR_WRITE: return "Error de escritura en flash";
    case UPDATE_ERROR_ERASE: return "Error de borrado de flash";
    case UPDATE_ERROR_READ: return "Error de lectura de flash";
    case UPDATE_ERROR_SPACE: return "Espacio insuficiente en partición OTA (revisar que el esquema sea min_spiffs)";
    case UPDATE_ERROR_SIZE: return "Tamaño de firmware inválido";
    case UPDATE_ERROR_STREAM: return "Error en stream de datos";
    case UPDATE_ERROR_MD5: return "Error de suma MD5";
    case UPDATE_ERROR_MAGIC_BYTE: return "Byte mágico inválido (el archivo no parece ser un binario ESP32)";
    case UPDATE_ERROR_ACTIVATE: return "Error al activar partición";
    case UPDATE_ERROR_NO_PARTITION: return "No se encontró partición OTA (usar esquema con OTA)";
    case UPDATE_ERROR_BAD_ARGUMENT: return "Argumento inválido";
    case UPDATE_ERROR_ABORT: return "Actualización abortada";
    default: return "Error de actualización (" + String(err) + ")";
  }
}

bool otaVerificarGitHub(String& nuevaVersion, String& urlDescarga, String& notas, String& err) {
  if (WiFi.status() != WL_CONNECTED) {
    err = "Sin conexión WiFi";
    return false;
  }

  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setUserAgent("AquaSense-ESP32");

  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases/latest";
  if (!http.begin(cliente, url)) {
    err = "No se pudo iniciar cliente HTTPS";
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    if (code == 404) err = "No se encontraron releases públicas en " + String(GITHUB_REPO);
    else if (code == 403) err = "Límite de peticiones de GitHub excedido (reintentar en unos minutos)";
    else err = "HTTP " + String(code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError jsonErr = deserializeJson(doc, payload);
  if (jsonErr) {
    err = "JSON inválido de GitHub: " + String(jsonErr.c_str());
    return false;
  }

  const char* tag = doc["tag_name"];
  if (!tag) {
    err = "Falta tag_name en release de GitHub";
    return false;
  }

  nuevaVersion = String(tag);
  nuevaVersion.trim();

  // URL por defecto directa de release
  urlDescarga = "https://github.com/" + String(GITHUB_REPO) + "/releases/latest/download/firmware.bin";

  // Buscar en los assets si hay un firmware.bin explícito
  if (doc["assets"].is<JsonArray>()) {
    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
      const char* name = asset["name"];
      if (name && (strcmp(name, "firmware.bin") == 0 || strcmp(name, "aquasense.bin") == 0)) {
        const char* bUrl = asset["browser_download_url"];
        if (bUrl) urlDescarga = String(bUrl);
        break;
      }
    }
  }

  const char* body = doc["body"];
  if (body) {
    notas = String(body);
    notas.trim();
    if (notas.length() > 250) notas = notas.substring(0, 250) + "...";
  } else {
    notas = "";
  }

  return true;
}

bool otaActualizarDesdeUrl(const String& url, String& err) {
  detenerTodo();
  for (int i = 0; i < N; i++) digitalWrite(PIN_BOMBA[i], LOW);

  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000);
  http.setUserAgent("AquaSense-ESP32");

  if (!http.begin(cliente, url)) {
    err = "No se pudo conectar a la URL";
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    err = "Error HTTP " + String(code);
    http.end();
    return false;
  }

  int len = http.getSize();
  if (len <= 0) {
    err = "Tamaño de firmware inválido";
    http.end();
    return false;
  }

  if (!Update.begin(len, U_FLASH)) {
    err = getUpdateErrorString();
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);

  if (written != (size_t)len) {
    err = "Escritura incompleta (" + String(written) + "/" + String(len) + " bytes)";
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end()) {
    err = getUpdateErrorString();
    http.end();
    return false;
  }

  if (!Update.isFinished()) {
    err = "Actualización no terminada correctamente";
    http.end();
    return false;
  }

  http.end();
  return true;
}

void iniciarOTA(WebServer& server) {
  server.on("/api/ota/info", HTTP_GET, [&server]() {
    String j = "{\"version\":\"" + String(AQUASENSE_VERSION) +
               "\",\"repo\":\"" + String(GITHUB_REPO) +
               "\",\"heap\":" + String(ESP.getFreeHeap()) + "}";
    server.send(200, "application/json", j);
  });

  server.on("/api/ota/check", HTTP_GET, [&server]() {
    String nVer, url, notas, err;
    bool ok = otaVerificarGitHub(nVer, url, notas, err);
    String j = "{";
    j += "\"ok\":" + String(ok ? "true" : "false");
    j += ",\"actual\":\"" + String(AQUASENSE_VERSION) + "\"";
    if (ok) {
      String actualLimpia = AQUASENSE_VERSION;
      String nuevaLimpia = nVer;
      if (actualLimpia.startsWith("v") || actualLimpia.startsWith("V")) actualLimpia = actualLimpia.substring(1);
      if (nuevaLimpia.startsWith("v") || nuevaLimpia.startsWith("V")) nuevaLimpia = nuevaLimpia.substring(1);
      bool esNueva = (actualLimpia != nuevaLimpia);

      j += ",\"disponible\":\"" + nVer + "\"";
      j += ",\"nueva\":" + String(esNueva ? "true" : "false");
      j += ",\"url\":\"" + url + "\"";
      j += ",\"notas\":\"" + jsonEsc(notas) + "\"";
    } else {
      j += ",\"error\":\"" + jsonEsc(err) + "\"";
    }
    j += "}";
    server.send(200, "application/json", j);
  });

  server.on("/api/ota/github", HTTP_POST, [&server]() {
    String nVer, url, notas, err;
    if (!otaVerificarGitHub(nVer, url, notas, err)) {
      server.send(500, "text/plain", "Error al consultar GitHub: " + err);
      return;
    }
    server.send(200, "text/plain", "Descargando e instalando actualización...");
    delay(500);
    if (otaActualizarDesdeUrl(url, err)) {
      Serial.println("OTA desde GitHub completada con éxito. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.printf("Error en OTA desde GitHub: %s\n", err.c_str());
    }
  });

  server.on("/api/ota/upload", HTTP_POST, [&server]() {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
      server.send(500, "text/plain", String("Error: ") + getUpdateErrorString());
    } else {
      server.send(200, "text/plain", "Actualización completada con éxito. Reiniciando el ESP32...");
      delay(1000);
      ESP.restart();
    }
  }, [&server]() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      detenerTodo();
      for (int i = 0; i < N; i++) digitalWrite(PIN_BOMBA[i], LOW);
      Serial.printf("OTA Local: iniciando archivo %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        Serial.printf("Error Update.begin: %s\n", getUpdateErrorString().c_str());
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Serial.printf("Error Update.write: %s\n", getUpdateErrorString().c_str());
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("OTA Local: subida exitosa (%u bytes)\n", upload.totalSize);
      } else {
        Serial.printf("Error Update.end: %s\n", getUpdateErrorString().c_str());
      }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      Serial.println("OTA Local: subida abortada");
    }
  });
}
