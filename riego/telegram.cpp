#define ARDUINOJSON_USE_LONG_LONG 1        // los ID de chat son de 64 bits
#include "telegram.h"
#include "config.h"
#include "utilidades.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct MsgTg { char texto[600]; };
struct CmdTg { char texto[80]; };

static QueueHandle_t colaSalida;           // mensajes a enviar
static QueueHandle_t colaEntrada;          // comandos recibidos
static SemaphoreHandle_t mtx;

bool tgReconfigurar = true;
char tgEstado[100] = "Desactivado";
char tgDesconocido[24] = "";

static const char COMANDOS_JSON[] = R"json({"commands":[
{"command":"estado","description":"Ver cómo están las macetas"},
{"command":"regar","description":"Regar: /regar 1, /regar kiwi o /regar todas"},
{"command":"detener","description":"Cortar el riego en curso"},
{"command":"pausa","description":"Pausar el riego automático"},
{"command":"reanudar","description":"Reanudar el riego automático"},
{"command":"horarios","description":"Ver los horarios de riego"},
{"command":"ayuda","description":"Ver la lista de comandos"}]})json";

void tgBloquear() { xSemaphoreTake(mtx, portMAX_DELAY); }
void tgLiberar()  { xSemaphoreGive(mtx); }

void iniciarColasTelegram() {
  mtx = xSemaphoreCreateMutex();
  colaSalida = xQueueCreate(6, sizeof(MsgTg));
  colaEntrada = xQueueCreate(4, sizeof(CmdTg));
}

void notificar(const String& texto) {
  if (!tg.activo) return;
  MsgTg m;
  strlcpy(m.texto, texto.c_str(), sizeof(m.texto));
  xQueueSend(colaSalida, &m, 0);
}

bool recibirComando(String& texto) {
  CmdTg c;
  if (xQueueReceive(colaEntrada, &c, 0) != pdTRUE) return false;
  texto = c.texto;
  return true;
}

static void setEstadoTg(const char* s) {
  tgBloquear();
  strlcpy(tgEstado, s, sizeof(tgEstado));
  tgLiberar();
}

static int tgPost(const char* token, const char* metodo, const String& cuerpo, String* respuesta,
                  const char* tipo = "application/x-www-form-urlencoded") {
  WiFiClientSecure cliente;
  cliente.setInsecure();                   // sin validar certificado (más simple)
  HTTPClient http;
  http.setTimeout(15000);
  String url = "https://api.telegram.org/bot";
  url += token;
  url += "/";
  url += metodo;
  if (!http.begin(cliente, url)) return -1;
  http.addHeader("Content-Type", tipo);
  int codigo = http.POST(cuerpo);
  if (respuesta && codigo > 0) *respuesta = http.getString();
  http.end();
  return codigo;
}

static void tareaTelegram(void*) {
  int64_t offset = 0;
  bool descartarViejos = true;             // no ejecutar comandos viejos al arrancar
  bool registrarComandos = false;
  char token[64], chat[24];
  MsgTg m;

  for (;;) {
    // Copia de la configuración. El pedido de reconfigurar se toma en el mismo
    // bloqueo, así nunca se usa un token viejo con un pedido nuevo.
    bool activo, comandos;
    tgBloquear();
    activo = tg.activo;
    comandos = tg.comandos;
    strlcpy(token, tg.token.c_str(), sizeof(token));
    strlcpy(chat, tg.chatId.c_str(), sizeof(chat));
    if (tgReconfigurar) {
      tgReconfigurar = false;
      registrarComandos = true;
      descartarViejos = true;
    }
    tgLiberar();

    if (!activo || token[0] == 0) {
      setEstadoTg(activo ? "Falta el token del bot" : "Desactivado");
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {}
      descartarViejos = true;
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      setEstadoTg("Sin conexión WiFi");
      vTaskDelay(pdMS_TO_TICKS(3000));
      continue;
    }

    if (registrarComandos) {
      int c = tgPost(token, "setMyCommands", COMANDOS_JSON, nullptr, "application/json");
      if (c != 200) {
        char b[100];
        if (c == 401 || c == 404) snprintf(b, sizeof(b), "Token inválido. Copialo de nuevo desde @BotFather");
        else snprintf(b, sizeof(b), "No se pudo conectar con Telegram (código %d)", c);
        setEstadoTg(b);
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
      registrarComandos = false;
      setEstadoTg(chat[0] ? "Conectado" : "Conectado. Falta autorizar tu chat");
    }

    // Enviar los mensajes pendientes
    if (chat[0]) {
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {
        String cuerpo = "chat_id=";
        cuerpo += chat;
        cuerpo += "&text=";
        cuerpo += urlEncode(m.texto);
        int c = tgPost(token, "sendMessage", cuerpo, nullptr);
        if (c == 200) {
          setEstadoTg("Conectado");
        } else {
          char b[100];
          if (c == 400 || c == 403) snprintf(b, sizeof(b), "No se pudo enviar. Revisá el ID del chat y que le hayas escrito al bot (código %d)", c);
          else snprintf(b, sizeof(b), "No se pudo enviar (código %d)", c);
          setEstadoTg(b);
        }
      }
    } else {
      while (xQueueReceive(colaSalida, &m, 0) == pdTRUE) {}
    }

    // Recibir mensajes (también sirve para detectar el ID de tu chat)
    if (!comandos && chat[0]) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    char cuerpoG[64];
    if (descartarViejos) {
      offset = 0;                          // el offset de otro bot no sirve
      snprintf(cuerpoG, sizeof(cuerpoG), "offset=-1&timeout=0");
    } else {
      snprintf(cuerpoG, sizeof(cuerpoG), "offset=%lld&timeout=8", (long long)offset);
    }
    String resp;
    int c = tgPost(token, "getUpdates", cuerpoG, &resp);
    if (c != 200) {
      if (c == 401 || c == 404) setEstadoTg("Token inválido. Copialo de nuevo desde @BotFather");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    JsonDocument filtro;
    filtro["result"][0]["update_id"] = true;
    filtro["result"][0]["message"]["chat"]["id"] = true;
    filtro["result"][0]["message"]["text"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, resp, DeserializationOption::Filter(filtro))) continue;

    for (JsonObject u : doc["result"].as<JsonArray>()) {
      offset = u["update_id"].as<int64_t>() + 1;
      if (descartarViejos) continue;
      int64_t cid = u["message"]["chat"]["id"].as<int64_t>();
      if (cid == 0) continue;
      const char* texto = u["message"]["text"] | "";
      char cidTexto[24];
      snprintf(cidTexto, sizeof(cidTexto), "%lld", (long long)cid);

      if (strcmp(cidTexto, chat) != 0) {
        // Chat sin autorizar: se informa su ID en la web y se le responde
        tgBloquear();
        strlcpy(tgDesconocido, cidTexto, sizeof(tgDesconocido));
        tgLiberar();
        String aviso = String("Este bot de riego todavía no te tiene autorizado. Tu ID de chat es ") + cidTexto +
                       ". Cargalo en la pestaña Notificaciones de la web del riego.";
        String cuerpo = String("chat_id=") + cidTexto + "&text=" + urlEncode(aviso.c_str());
        tgPost(token, "sendMessage", cuerpo, nullptr);
        continue;
      }
      if (!comandos) continue;
      CmdTg cmd;
      strlcpy(cmd.texto, texto, sizeof(cmd.texto));
      xQueueSend(colaEntrada, &cmd, 0);
    }
    descartarViejos = false;
  }
}

void iniciarTareaTelegram() {
  // Corre en el núcleo 0 para no demorar el riego ni la web
  xTaskCreatePinnedToCore(tareaTelegram, "telegram", 12288, nullptr, 1, nullptr, 0);
}
