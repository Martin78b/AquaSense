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
3d/                        modelos para impresión 3D
└── carcasa_aquasense.scad caja paramétrica sin tornillos (shield 78x69 mm + ULN2003)
assets/                    gráficos del proyecto (logo oficial en SVG)
docs/                      documentación técnica y esquemas
├── Cableado AquaSense.html plano interactivo SVG / HTML del circuito
├── conexiones.md          guía de cableado con alargador de 2 puertos USB
├── especificacion-riego.md especificación completa de hardware y lógica
└── riego_web.ino          implementación original monolítica de referencia
riego/                     carpeta del sketch (abrí riego.ino con el Arduino IDE)
├── riego.ino              setup() y loop()
├── config.h / .cpp        pines, valores por defecto y persistencia (Preferences)
├── control_riego.h / .cpp cola de riego, bombas, horarios, sensores y botón BOOT
├── sensores.h / .cpp      lectura y conversión a porcentaje
├── servidor_web.h / .cpp  servidor web y API JSON
├── pagina_web.h           página HTML con CSS y JavaScript embebidos
├── telegram.h / .cpp      tarea de Telegram (núcleo 0), colas y mutex
├── comandos.h / .cpp      comandos y textos de Telegram
├── ota.h / .cpp           actualizaciones OTA (web local, GitHub Releases y Telegram)
├── utilidades.h / .cpp    hora, JSON y URL
├── secrets.example.h      plantilla de credenciales del WiFi
└── secrets.h              tus credenciales (no se versiona)
platformio.ini             configuración para compilar con PlatformIO
```

## Lista de materiales

| Componente | Detalle |
|---|---|
| ESP32 | NodeMCU ESP-32S de 38 pines (Ai-Thinker ESP-32S, equivalente a WROOM-32), USB CP2102 |
| Driver de bombas | Módulo ULN2003 (el de los motores 28BYJ-48). Se usan 3 de sus 4 canales |
| Bombas | 3 mini bombas sumergibles de 3 a 5 V DC |
| Sensores de humedad | 3 sensores capacitivos V2.0.0 (tipo TLC555) |
| Sensor de nivel del bidón (futuro) | Flotante o sin contacto XKC-Y25 |
| Capacitor | Electrolítico de 1000 µF y 16 V (mínimo 10 V) |
| Fuente del ESP32 | Cargador de celular de 5 V, por el micro USB |
| Fuente de las bombas | Cargador de Raspberry Pi 4 (5,1 V y 3 A) con una placa breakout USB-C hembra con resistencias de 5,1 kΩ |
| Manguera | Silicona, generalmente 6 mm interior × 8 mm exterior |
| Bidón | Más bajo que las macetas, para evitar el efecto sifón |

## Conexiones

Podés ver el plano interactivo de cableado en [`docs/Cableado AquaSense.html`](docs/Cableado AquaSense.html). Si alimentás todo desde un alargador con dos puertos USB, mirá [`docs/conexiones.md`](docs/conexiones.md).

```
 CARGADOR DE CELULAR 5 V                    CARGADOR RASPBERRY PI 5,1 V 3 A
        │ micro USB                                  │ USB-C (placa breakout)
        ▼                                            ▼
┌───────────────────────┐                     + ─────┬───────────► + ULN2003
│  ESP32 NodeMCU-32S    │                            ├───────────► + de las 3 bombas (rojo)
│                       │                            ═╧═ 1000 µF
│                       │                     − ─────┴───────────► − ULN2003
│  GPIO 25 ─────────────┼──► IN1 ULN2003                              ▲
│  GPIO 26 ─────────────┼──► IN2 ULN2003                              │
│  GPIO 27 ─────────────┼──► IN3 ULN2003                              │
│  GND ─────────────────┼─────────────────── masas unidas ────────────┘
│  VIN (sin conectar)   │
│                       │     ULN2003, conector blanco:
│  3V3 ─────────────────┼──►  VCC de los 3 sensores     A ─────────────► − bomba 1
│  GND ─────────────────┼──►  GND de los 3 sensores     B ─────────────► − bomba 2
│  GPIO 32 ◄────────────┼───  AOUT sensor 1             C ─────────────► − bomba 3
│  GPIO 33 ◄────────────┼───  AOUT sensor 2             (C+ queda sin usar)
│  GPIO 34 ◄────────────┼───  AOUT sensor 3
│  GPIO 14 ◄────────────┼───  sensor de nivel (futuro, a GND, pull-up interno)
│  BOOT (GPIO 0): riego manual de las tres macetas
└───────────────────────┘
```

| Elemento | Conexión |
|---|---|
| Bomba 1, 2, 3 (control) | GPIO 25, 26, 27 → IN1, IN2, IN3 del ULN2003 |
| Bombas (positivo) | Positivo (+5 V) directo de la fuente de energía (cable rojo compartido; no toman del pin C+ del ULN2003) |
| Bombas (negativo) | Salidas A, B y C del conector blanco del ULN2003 (el pin común C+ queda sin usar) |
| ULN2003 "+" y "−" | 5,1 V y GND del cargador de la Raspberry Pi, con el jumper de alimentación puesto |
| Capacitor de 1000 µF | Entre + y − en la entrada del ULN2003 (respetá la polaridad) |
| Sensores de humedad (señal) | GPIO 32, 33, 34 (ADC1, que funciona con el WiFi activo) |
| Sensores de humedad (alimentación) | 3,3 V y GND del ESP32 (los sensores V2.0.0 con TLC555 funcionan nativamente a 3,3 V y quedan dentro del rango del ADC) |
| Sensor de nivel (futuro) | GPIO 14 y GND |

Reglas de alimentación:

- **Alimentación de las bombas:** el cable positivo (rojo) de las tres bombas se conecta directamente a los +5 V de la fuente de alimentación externa (cable rojo de la fuente), no desde el pin común C+ del conector blanco del ULN2003, evitando saturar las pistas de la placa driver.
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
7. En **Herramientas > Partition Scheme**, seleccioná **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** para habilitar actualizaciones OTA.
8. Tocá **Subir**. Si se queda en "Connecting…", mantené apretado el botón BOOT hasta que empiece a escribir.
9. Abrí el monitor serie a **115200 baudios**: ahí aparece la IP. Entrá a `http://riego.local` o a esa IP desde un dispositivo de la misma red WiFi.

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

`riego/secrets.h` está en `.gitignore`. Al arrancar por primera vez, el ESP32 guarda el SSID y la contraseña en su memoria interna no volátil (**NVS**). A partir de ese momento, los datos de red sobreviven a cualquier actualización OTA (incluso si descargás una versión pública genérica desde GitHub Releases). También podés cambiar de red WiFi desde la pestaña **Sistema** en la web sin conectar cables. El ESP32 solo se conecta a redes de 2,4 GHz.

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
| `POST /api/wifi` | Guarda nueva red WiFi en NVS y reinicia |
| `GET /api/ota/info` | Información del sistema (versión, repo y memoria) |
| `GET /api/ota/check` | Consulta la última release en GitHub |
| `POST /api/ota/github` | Descarga e instala release desde GitHub |
| `POST /api/ota/upload` | Subida e instalación de binario local (`.bin`) |

La web no tiene contraseña: cualquier dispositivo de tu red WiFi puede usarla.

## Actualizaciones inalámbricas (OTA)

AquaSense permite actualizar el firmware por WiFi de tres formas distintas (todas detienen automáticamente las bombas y reinician la placa de forma segura):

1. **Desde la Web con GitHub Releases:** En la pestaña **Sistema**, hacé click en **Buscar actualización en GitHub**. Si publicaste una versión más nueva con su `firmware.bin` adjunto, muestra las notas de versión y un botón para instalarla directamente.
2. **Desde la Web con archivo local:** En la misma pestaña **Sistema**, seleccioná el archivo `firmware.bin` generado en tu computadora y hacé click en **Subir e instalar**. Muestra una barra de progreso en tiempo real.
3. **Desde Telegram:** Enviá `/actualizar` para comprobar si hay una nueva versión. Si existe, respondé `/actualizar confirmar` para iniciar la descarga e instalación.

> [!NOTE]
> Requiere compilar con esquema de particiones **Minimal SPIFFS (1.9MB APP with OTA)** en Arduino IDE o `board_build.partitions = min_spiffs.csv` en PlatformIO (ya configurado por defecto).

## Carcasa para impresión 3D

En la carpeta [`3d/`](3d/) se incluye el modelo paramétrico [`carcasa_aquasense.scad`](3d/carcasa_aquasense.scad), compatible con [OpenSCAD](https://openscad.org/) o visores online como [ochafik.com/openscad2](https://ochafik.com/openscad2):

- **100% sin tornillos:** las placas y la tapa calzan por presión (snap-fit). Las guías en "L" y clips elásticos aseguran el breakout shield del ESP32 (78 mm × 69 mm) y el módulo ULN2003 (35 mm × 32 mm).
- **Apertura práctica:** tapa con cuatro trabas internas y muescas laterales para abrir fácil con los dedos.
- **Conectividad:** calado a medida para el conector USB-C del ESP32 y orificio para ficha hembra Jack DC 5.5×2.1 mm.
- **Organización interna:** cuna elástica dedicada para el capacitor electrolítico de 1000 µF, ventana de inspección para los LEDs del ULN2003 y 6 ranuras pasacables frontales en "U" (3 para sensores y 3 para bombas).
- **Logo oficial:** grabado en relieve en la cara superior de la tapa.
- **Fabricación:** diseñado para imprimirse sin soportes en tecnología FDM (PLA / PETG).

## Pendientes

- Sensor de nivel del bidón en GPIO 14: bloquear las bombas si no hay agua, mostrarlo en la web y avisar por Telegram.
- Piezas 3D complementarias: tapa adaptadora del bidón para mangueras, capuchón protector de sensores, soportes y difusores de maceta (la caja principal ya está disponible en [`3d/`](3d/)).
- Validar el certificado de Telegram en lugar de `setInsecure()`.

## Licencia

MIT. Ver [LICENSE](LICENSE). El logo (`assets/logo.svg`) es un diseño original del proyecto y tiene la misma licencia.
