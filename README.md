<p align="center"><img src="assets/logo.svg" alt="Logo de AquaSense: una gota de agua con un brote saliendo de la tierra" width="160"></p>

# AquaSense

Riego automático para tres macetas con un ESP32, tres mini bombas sumergibles y sensores capacitivos de humedad. Riega por horario o según la humedad del suelo, se maneja desde una página web servida por el propio ESP32 en tu red local (`http://riego.local`) y te avisa y recibe comandos por Telegram.

La especificación completa está en [`docs/especificacion-riego.md`](docs/especificacion-riego.md). El archivo [`docs/riego_web.ino`](docs/riego_web.ino) es la implementación original en un solo archivo, que se conserva como referencia.

## Qué hace

- Riega una cantidad fija de ml por maceta, calculando el tiempo con el caudal de cada bomba y sin pasar nunca el tiempo máximo de seguridad.
- Nunca enciende dos bombas a la vez: los pedidos entran en una cola y se atienden de a uno, con 2 s de pausa entre bombas.
- Las macetas **sin sensor** riegan en los horarios activos (hasta 4). Las macetas **con sensor** riegan cuando la humedad baja del umbral y ya pasó la espera mínima.
- Tiene un máximo de riegos automáticos por día por maceta, como protección si falla un sensor.
- El riego automático se puede pausar (sobrevive a cortes de luz). Los riegos manuales siguen funcionando.
- El botón BOOT de la placa riega las tres macetas.
- Telegram: aviso de cada riego, aviso al llegar al máximo diario, aviso de encendido, resumen diario y comandos (`/estado`, `/regar`, `/detener`, `/pausa`, `/reanudar`, `/horarios`, `/ayuda`). Solo obedece a tu chat.

## Estructura del repositorio

```
riego/                     carpeta del sketch (abrí riego.ino con el Arduino IDE)
├── riego.ino              setup() y loop()
├── config.h / .cpp        pines, valores por defecto y persistencia (Preferences)
├── control_riego.h / .cpp cola de riego, bombas, horarios, sensores y botón BOOT
├── sensores.h / .cpp      lectura y conversión a porcentaje
├── servidor_web.h / .cpp  servidor web y API JSON
├── pagina_web.h           página HTML con CSS y JavaScript embebidos
├── telegram.h / .cpp      tarea de Telegram (núcleo 0), colas y mutex
├── comandos.h / .cpp      comandos y textos de Telegram
├── utilidades.h / .cpp    hora, JSON y URL
├── secrets.example.h      plantilla de credenciales del WiFi
└── secrets.h              tus credenciales (no se versiona)
docs/                      especificación e implementación de referencia
platformio.ini             alternativa al Arduino IDE
```

## Lista de materiales

| Componente | Detalle |
|---|---|
| ESP32 | NodeMCU ESP-32S de 38 pines (Ai-Thinker ESP-32S, equivalente a WROOM-32), USB CP2102 |
| Driver de bombas | Módulo ULN2003 (el de los motores 28BYJ-48). Se usan 3 de sus 4 canales |
| Bombas | 3 mini bombas sumergibles de 3 a 5 V DC |
| Sensores de humedad | 3 sensores capacitivos v1.2 |
| Sensor de nivel del bidón (futuro) | Flotante o sin contacto XKC-Y25 |
| Capacitor | Electrolítico de 1000 µF y 16 V (mínimo 10 V) |
| Fuente del ESP32 | Cargador de celular de 5 V, por el micro USB |
| Fuente de las bombas | Cargador de Raspberry Pi 4 (5,1 V y 3 A) con una placa breakout USB-C hembra con resistencias de 5,1 kΩ |
| Manguera | Silicona, generalmente 6 mm interior × 8 mm exterior |
| Bidón | Más bajo que las macetas, para evitar el efecto sifón |

## Conexiones

```
 CARGADOR DE CELULAR 5 V                    CARGADOR RASPBERRY PI 5,1 V 3 A
        │ micro USB                                  │ USB-C (placa breakout)
        ▼                                            ▼
┌───────────────────────┐                     + ─────┬───────────► + ULN2003
│  ESP32 NodeMCU-32S    │                            ═╧═ 1000 µF
│                       │                     − ─────┴───────────► − ULN2003
│  GPIO 25 ─────────────┼──► IN1 ULN2003                              ▲
│  GPIO 26 ─────────────┼──► IN2 ULN2003                              │
│  GPIO 27 ─────────────┼──► IN3 ULN2003                              │
│  GND ─────────────────┼─────────────────── masas unidas ────────────┘
│  VIN (sin conectar)   │
│                       │     ULN2003, conector blanco:
│  3V3 ─────────────────┼──►  VCC de los 3 sensores     común (rojo) ──► + de las 3 bombas
│  GND ─────────────────┼──►  GND de los 3 sensores     A ─────────────► − bomba 1
│  GPIO 32 ◄────────────┼───  AOUT sensor 1             B ─────────────► − bomba 2
│  GPIO 33 ◄────────────┼───  AOUT sensor 2             C ─────────────► − bomba 3
│  GPIO 34 ◄────────────┼───  AOUT sensor 3
│  GPIO 14 ◄────────────┼───  sensor de nivel (futuro, a GND, pull-up interno)
│  BOOT (GPIO 0): riego manual de las tres macetas
└───────────────────────┘
```

| Elemento | Conexión |
|---|---|
| Bomba 1, 2, 3 (control) | GPIO 25, 26, 27 → IN1, IN2, IN3 del ULN2003 |
| Bombas (positivo) | Pin común del conector blanco del ULN2003 (el del cable rojo del motor) |
| Bombas (negativo) | Salidas A, B y C del conector blanco |
| ULN2003 "+" y "−" | 5,1 V y GND del cargador de la Raspberry Pi, con el jumper de alimentación puesto |
| Capacitor de 1000 µF | Entre + y − en la entrada del ULN2003 (respetá la polaridad) |
| Sensores de humedad (señal) | GPIO 32, 33, 34 (ADC1, que funciona con el WiFi activo) |
| Sensores de humedad (alimentación) | 3,3 V y GND del ESP32, para no superar el rango del ADC |
| Sensor de nivel (futuro) | GPIO 14 y GND |

Reglas de alimentación:

- **Uní las masas:** un cable entre el GND del ESP32 y el "−" del ULN2003. Sin eso, el ULN2003 no interpreta las señales.
- **Nunca unas los positivos:** el pin VIN del ESP32 queda sin conectar. Así podés enchufar el ESP32 a la compu para programarlo sin desconectar las bombas.
- No uses los GPIO 6 a 11 (CLK, SD0 a SD3 y CMD): están conectados a la flash.
- Las bombas no tienen que funcionar nunca en seco.

## Instalación

### Arduino IDE

1. En **Archivo > Preferencias > Gestor de URLs adicionales de tarjetas**, agregá:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. En **Herramientas > Placa > Gestor de tarjetas**, buscá **esp32** (de Espressif Systems) e instalalo.
3. En **Herramientas > Administrar bibliotecas**, buscá **ArduinoJson** (de Benoit Blanchon) e instalá la **versión 7**.
4. Cargá tus credenciales (ver abajo).
5. Abrí `riego/riego.ino`. Se abren todas las pestañas del proyecto.
6. Elegí la placa **NodeMCU-32S** (o **ESP32 Dev Module**) y el puerto. Si no aparece el puerto, instalá el driver del CP2102.
7. Tocá **Subir**. Si se queda en "Connecting…", mantené apretado el botón BOOT hasta que empiece a escribir.
8. Abrí el monitor serie a **115200 baudios**: ahí aparece la IP. Entrá a `http://riego.local` o a esa IP desde un dispositivo de la misma red WiFi.

### Con arduino-cli

```sh
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s riego
arduino-cli upload  --fqbn esp32:esp32:nodemcu-32s -p /dev/cu.usbserial-XXXX riego
```

### Con PlatformIO (alternativa)

```sh
pio run                 # compila
pio run -t upload       # sube
pio device monitor      # monitor serie a 115200
```

## Credenciales del WiFi

Las credenciales no se guardan en el repositorio. Copiá la plantilla y completala:

```sh
cp riego/secrets.example.h riego/secrets.h
```

```c
#define WIFI_SSID "nombre de tu red"
#define WIFI_PASS "tu clave"
```

`riego/secrets.h` está en `.gitignore`. Si falta, la compilación se corta con un mensaje que te indica copiarlo. El ESP32 solo se conecta a redes de 2,4 GHz.

## Puesta en marcha

### 1. Medí el caudal de cada bomba

El sistema calcula cuánto tiempo regar a partir del caudal, así que hay que medirlo para cada bomba, con la manguera ya puesta y el bidón a la altura real:

1. En la pestaña de la maceta, poné provisoriamente **Agua por riego = 200 ml** y **Caudal = 20 ml/s**, y guardá. Eso da un riego de 10 s.
2. Poné la punta de la manguera en un vaso medidor y tocá **Regar** en la pestaña Estado.
3. Dividí los ml que juntaste por 10. Ese es el caudal en ml/s. Por ejemplo, 180 ml en 10 s son 18 ml/s.
4. Cargá ese caudal en **Caudal medido de la bomba**, volvé a poner los ml que querés por riego y guardá.

Repetilo con cada bomba: aunque sean iguales, el largo y la altura de cada manguera cambian el caudal.

### 2. Calibrá los sensores

Los sensores vienen **desactivados**: mientras no estén conectados, los pines quedan al aire y dan lecturas al azar, que no deben disparar riegos.

1. Conectá el sensor y clavalo en la maceta con la **tierra seca**. Siempre a la misma profundidad, sin mojar la zona de componentes.
2. En la pestaña de la maceta, tocá **Usar actual** al lado de **Lectura con tierra seca**.
3. Regá bien la maceta, esperá unos minutos y tocá **Usar actual** al lado de **Lectura con tierra mojada**. Los capacitivos dan valores más altos en seco y más bajos en mojado.
4. Recién ahí activá **Regar según el sensor**, revisá el umbral y guardá.

Desde ese momento la maceta riega por humedad (cada 10 s se revisa) y deja de usar los horarios.

### 3. Configurá el bot de Telegram

1. En Telegram, buscá **@BotFather**, mandale `/newbot` y seguí los pasos. Al final te da un **token** del estilo `123456789:AAH4k2xYz...`.
2. En la web del riego, pestaña **Notificaciones**: pegá el token, activá **Activar Telegram** y tocá **Guardar ajustes**.
3. Escribile cualquier mensaje a tu bot. Te va a responder con tu ID de chat y en la web aparece un botón **Autorizar**.
4. Tocá **Autorizar** y después **Guardar ajustes**.
5. Probá con **Enviar mensaje de prueba**.

El token nunca se devuelve a la web: queda guardado en el ESP32 y el campo se muestra vacío ("Token guardado. Dejalo vacío para no cambiarlo."). Solo tu chat autorizado puede dar órdenes. En grupos también funcionan los comandos del estilo `/estado@NombreDelBot`. Los comandos pueden tardar hasta unos 10 s en responder.

## Valores por defecto

| Maceta | ml por riego | Umbral | Espera mínima | Máx. riegos automáticos por día |
|---|---|---|---|---|
| Hortensia A | 400 | 55 % | 120 min | 3 |
| Hortensia B | 400 | 55 % | 120 min | 3 |
| Kiwi C | 150 | 38 % | 60 min | 2 |

Caudal 20 ml/s, calibración 2800 en seco y 1200 en mojado, sensores desactivados, tiempo máximo de un riego 60 s, horarios activos a las 08:00 y 19:00 (y 13:00 y 22:00 desactivados), resumen diario a las 21:00. Zona horaria de Argentina (UTC-3), con hora por NTP.

## API

| Método y ruta | Función |
|---|---|
| `GET /` | Página web |
| `GET /api/estado` | Estado general y de cada maceta |
| `GET /api/config` | Configuración (sin el token de Telegram) |
| `POST /api/config` | Guarda la configuración (form-urlencoded) |
| `POST /api/regar` | `m` = índice de la maceta |
| `POST /api/detener` | Detiene el riego y vacía la cola |
| `POST /api/pausa` | `on` = 1 o 0 |
| `POST /api/telegram/prueba` | Envía un mensaje de prueba |

La web no tiene contraseña: cualquier dispositivo de tu red WiFi puede usarla.

## Pendientes

- Sensor de nivel del bidón en GPIO 14: bloquear las bombas si no hay agua, mostrarlo en la web y avisar por Telegram.
- Piezas para imprimir en 3D (PETG o ASA): caja, tapa del bidón, capuchón de los sensores, soportes y difusores.
- Validar el certificado de Telegram en lugar de `setInsecure()`.
- Actualización por WiFi (OTA). El firmware ocupa cerca del 87 % de la partición por defecto, así que para OTA conviene elegir un esquema de particiones más grande (por ejemplo "Minimal SPIFFS").

## Licencia

MIT. Ver [LICENSE](LICENSE). El logo (`assets/logo.svg`) es un diseño original del proyecto y tiene la misma licencia.
