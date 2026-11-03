# Sistema de riego automático con ESP32: especificación del proyecto

Este documento resume las decisiones tomadas para el proyecto y sirve como especificación para generar el código del repositorio. Junto con este archivo se entrega `riego_web.ino`, una implementación de referencia completa y funcional en un solo archivo. El objetivo es organizar ese código en un repositorio prolijo, manteniendo exactamente el comportamiento descrito acá.

Todo el texto visible para el usuario (página web, mensajes de Telegram, monitor serie) va en **español rioplatense con voseo** ("regá", "revisá", "tocá").

---

## 1. Objetivo

Regar automáticamente tres macetas desde un bidón, con tres mini bombas sumergibles controladas por un ESP32. El sistema riega por horario o según sensores de humedad de suelo, se controla desde una página web servida por el propio ESP32 en la red local, y envía notificaciones y recibe comandos por Telegram.

---

## 2. Hardware

### Componentes

| Componente | Detalle | Estado |
|---|---|---|
| ESP32 | NodeMCU ESP-32S, 38 pines (módulo Ai-Thinker ESP-32S, equivalente a WROOM-32). Chip USB CP2102. En Arduino IDE: placa "NodeMCU-32S" o "ESP32 Dev Module" | Disponible |
| Driver de bombas | Módulo ULN2003 (el de los motores paso a paso 28BYJ-48). Se usan 3 de sus 4 canales | Disponible |
| Bombas | 3 mini bombas sumergibles de 3 a 5 V DC, dentro del bidón | Disponibles |
| Sensores de humedad | 3 sensores capacitivos v1.2 | En camino |
| Sensor de nivel del bidón | Flotante o sin contacto XKC-Y25 | Pendiente de compra |
| Capacitor | Electrolítico 1000 µF, 16 V (mínimo 10 V) | Pendiente de compra |
| Alimentación | Cargador de celular 5 V (para el ESP32) y cargador oficial de Raspberry Pi 4 de 5,1 V y 3 A (para las bombas), este último mediante una placa breakout USB-C hembra con resistencias de 5,1 kΩ | Disponibles |
| Piezas impresas en 3D | El usuario tiene impresora 3D. Material PETG o ASA (no PLA) | Pendientes de diseño |

### Alimentación

Se usan **dos fuentes separadas**:

- **Cargador de celular → ESP32**, por su conector micro USB.
- **Cargador de Raspberry Pi → bombas**, a través de la placa USB-C y el ULN2003.

Reglas: las **masas (GND) de ambos circuitos se unen** con un cable entre el GND del ESP32 y el pin "−" del ULN2003 (imprescindible para que el ULN2003 interprete las señales). Los **positivos nunca se unen**: el pin VIN del ESP32 queda sin conectar. El capacitor de 1000 µF va entre + y − en la entrada del ULN2003, para absorber el pico de arranque de las bombas. Esta separación permite conectar el ESP32 a la PC para programarlo sin desconectar las bombas.

### Conexiones (pinout)

| Elemento | Conexión |
|---|---|
| Bomba 1, 2, 3 (control) | GPIO 25, 26, 27 → IN1, IN2, IN3 del ULN2003 |
| Bombas (positivo) | Positivo (+5 V) de la fuente de energía (cable rojo compartido; no toman del pin C+ del ULN2003) |
| Bombas (negativo) | Salidas A, B y C del conector blanco del ULN2003 (pin C+ libre) |
| ULN2003 "+" y "−" | 5,1 V y GND del cargador de la Raspberry Pi (vía placa USB-C), con el jumper de alimentación puesto |
| Sensores de humedad (señal) | GPIO 32, 33, 34 (todos de ADC1, que funciona con WiFi activo) |
| Sensores de humedad (alimentación) | 3,3 V y GND del ESP32 (a 3,3 V para no superar el rango del ADC) |
| Sensor de nivel (futuro) | GPIO 14 y GND, con pull-up interno |
| Botón de riego manual | Botón BOOT de la placa (GPIO 0) |

Pines a evitar en esta placa: GPIO 6 a 11 (rotulados CLK, SD0 a SD3 y CMD, conectados a la flash).

### Notas hidráulicas (no afectan el código)

El bidón debe quedar más bajo que las macetas para evitar el efecto sifón (o hacer un agujerito en la manguera, dentro del bidón, que lo corte). Las bombas no deben funcionar nunca en seco. Manguera de silicona, generalmente 6 mm interior × 8 mm exterior.

---

## 3. Macetas y valores por defecto

| Índice | Nombre | Planta | ml por riego | Umbral de humedad | Espera mínima entre riegos | Máx. riegos automáticos por día |
|---|---|---|---|---|---|---|
| 0 | Hortensia A | Hortensia en maceta grande | 400 | 55 % | 120 min | 3 |
| 1 | Hortensia B | Hortensia en maceta grande | 400 | 55 % | 120 min | 3 |
| 2 | Kiwi C | Kiwi en maceta chica (sensible al encharcamiento) | 150 | 38 % | 60 min | 2 |

Otros valores por defecto: caudal de cada bomba 20 ml/s (el usuario debe medirlo y cargarlo), calibración del sensor 2800 en seco y 1200 en mojado, sensores desactivados, tiempo máximo de un riego 60 s, horarios activos a las 08:00 y 19:00 y dos horarios más desactivados (13:00 y 22:00). Zona horaria Argentina, `<-03>3`, con hora por NTP (`pool.ntp.org`, `time.google.com`).

---

## 4. Comportamiento del riego

- **Cantidad por riego:** se configura en ml. La duración se calcula como `ml / caudal` y se limita al tiempo máximo de seguridad. Los ml efectivamente regados se calculan con la duración final.
- **Una bomba por vez, siempre.** Los pedidos de riego entran en una cola (bandera "pendiente" por maceta) y se atienden de a uno, con una pausa de 2 s entre bombas. Antes de encender cualquier bomba se apagan todas.
- **Riego no bloqueante:** se controla con `millis()`, nunca con `delay()` largos, para que la web y Telegram sigan respondiendo.
- **Macetas sin sensor:** riegan en los horarios activos (hasta 4 horarios configurables), cada horario una vez por día.
- **Macetas con sensor:** se revisan cada 10 s. Riegan si la humedad está por debajo del umbral y pasó la espera mínima desde su último riego (manual o automático). Los horarios no se aplican a estas macetas.
- **Máximo de riegos automáticos por día** por maceta, como protección ante sensores fallados. Los riegos manuales no cuentan para este máximo.
- **Contadores diarios** (riegos automáticos y ml del día) se reinician al cambiar el día según NTP. Si no hay hora, se reinician cada 24 h de funcionamiento.
- **Pausa del riego automático:** frena horarios y sensores, pero los riegos manuales siguen funcionando. Se guarda en memoria y sobrevive a cortes de luz.
- **Detener:** apaga la bomba en curso y vacía la cola.
- **Botón BOOT:** pone en cola las tres macetas.
- **Origen de cada riego**, para mostrarlo en las notificaciones: horario, sensor, web, Telegram o botón.

---

## 5. Sensores de humedad

- Lectura promediada de 8 muestras, cada 2 s, resolución de 12 bits (0 a 4095).
- Conversión a porcentaje con la calibración de cada sensor: `map(lectura, seco, mojado, 0, 100)`, limitado a 0 a 100. Si seco y mojado son iguales, el porcentaje es 0.
- Los sensores capacitivos dan valores **más altos en seco** y más bajos en mojado.
- **Vienen desactivados.** Mientras no estén conectados, los pines quedan al aire y dan lecturas al azar; eso no debe disparar riegos.
- Calibración desde la web: con la tierra seca se toca "Usar actual" en la lectura seca; después de regar, lo mismo en la lectura mojada. Recién entonces se activa "Regar según el sensor".

---

## 6. Página web

Servida por el ESP32 en `http://riego.local` (mDNS) o por su IP. Una sola página HTML con CSS y JavaScript embebidos, sin recursos externos (debe funcionar sin internet). Se actualiza el estado cada 3 s.

### Diseño

- Paleta inspirada en el jardín: verde hoja como color principal, azul hortensia para el agua y la humedad, marrón tierra para la marca del umbral, rojo ladrillo para detener y errores.
- **Modo oscuro:** un botón en el encabezado alterna entre automático (sigue al sistema), claro y oscuro. La elección se guarda en `localStorage` (con try/catch).
- Pensada para el celular: ancho máximo de unos 620 px.

### Pestañas

Debajo del título hay una fila de pestañas tipo "píldora" que **se acomodan en varias filas** si no entran (sin desplazamiento horizontal). Las de Estado, Horarios y Notificaciones están escritas en el HTML para que se vean aunque la configuración no haya cargado; las de cada maceta se generan con el nombre configurado.

1. **Estado** (la que se abre por defecto): una tarjeta por maceta con nombre, barra de humedad con marca del umbral (si usa sensor) o "Riego por horario" y la lectura cruda, ml regados hoy, riegos automáticos de hoy, último riego o estado actual ("Regando, faltan N s" o "En espera para regar"), y un botón "Regar N ml" que se deshabilita mientras esa maceta riega o espera. Abajo, el botón "Detener riego" y el de pausar o reanudar el riego automático. Si está en pausa, se muestra un aviso.
2. **Horarios:** los 4 horarios (activo y hora) y el tiempo máximo de un riego.
3. **Notificaciones:** configuración de Telegram (ver sección 7).
4. **Una pestaña por maceta:** nombre, ml por riego, caudal medido (ml/s), usar sensor, umbral (%), lectura en seco y mojado (cada una con botón "Usar actual"), espera mínima (min) y máximo de riegos automáticos por día.

El botón **"Guardar ajustes"** aparece en todas las pestañas menos Estado y guarda todo junto. La validación del lado del navegador muestra un mensaje claro, salta a la pestaña del campo con error y le da foco. Rangos: ml 10 a 3000, caudal 1 a 200, umbral 0 a 100, lecturas 0 a 4095 (seco y mojado distintas), espera 0 a 1440, máximo por día 0 a 24, tiempo máximo 1 a 300 s, nombre obligatorio de hasta 20 caracteres. El servidor vuelve a limitar todos los valores.

### API

| Método y ruta | Función |
|---|---|
| `GET /` | Página |
| `GET /api/estado` | Hora, RSSI, maceta regando, segundos restantes, pausa, estado de Telegram, último chat no autorizado y, por maceta: nombre, lectura cruda, %, usa sensor, riegos de hoy, ml de hoy, ml por riego, pendiente y último riego |
| `GET /api/config` | Toda la configuración. **El token de Telegram nunca se envía**, solo un booleano `tokenGuardado` |
| `POST /api/config` | Guarda la configuración (form-urlencoded). El token solo se reemplaza si llega uno nuevo no vacío |
| `POST /api/regar` | `m` = índice de maceta, riego manual |
| `POST /api/detener` | Detiene todo |
| `POST /api/pausa` | `on` = 1 o 0 |
| `POST /api/telegram/prueba` | Envía un mensaje de prueba, o devuelve 400 con el motivo |

La web no tiene contraseña: cualquier dispositivo de la red WiFi puede usarla.

---

## 7. Telegram

### Arquitectura

- Corre en una **tarea FreeRTOS propia, fijada al núcleo 0**, con 12 KB de pila, para que las demoras de red no afecten el riego ni la web.
- Comunicación con el programa principal mediante **dos colas**: mensajes salientes (textos de hasta 600 caracteres, 6 lugares) y comandos entrantes (hasta 80 caracteres, 4 lugares). La configuración de Telegram y los textos de estado compartidos se protegen con un **mutex**. Toda la lógica de riego y el armado de respuestas se ejecutan en el programa principal.
- HTTPS con `WiFiClientSecure` y `HTTPClient`; por ahora con `setInsecure()` (sin validar el certificado).
- Recepción por long polling (`getUpdates` con `timeout=8`). Los comandos pueden tardar hasta unos 10 s.
- Librería **ArduinoJson v7** con filtro para leer `update_id`, `message.chat.id` y `message.text`. Los ID de chat son de 64 bits (`ARDUINOJSON_USE_LONG_LONG 1`).
- Al arrancar o al cambiar la configuración, **descarta los mensajes viejos** (`offset=-1`) para no ejecutar comandos enviados mientras estaba apagado, y registra los comandos en el menú del bot con `setMyCommands`.

### Seguridad

- Solo se aceptan comandos del **chat autorizado**. Si escribe otro chat, se le responde con su ID y que no está autorizado, y ese ID se muestra en la web con un botón "Autorizar" (que carga el ID en el campo; después hay que guardar).
- El token se guarda en memoria, nunca se devuelve por la API, y el campo se muestra vacío con el texto "Token guardado. Dejalo vacío para no cambiarlo."

### Configuración (pestaña Notificaciones)

Activar Telegram, token del bot (campo tipo contraseña), ID del chat autorizado, estado de la conexión (por ejemplo "Conectado", "Desactivado", "Falta el token del bot", "Token inválido. Copialo de nuevo desde @BotFather", "Sin conexión WiFi"), aceptar comandos, y avisos activables por separado:

- Cada vez que se riega una maceta, con los ml y el origen.
- Cuando una maceta con sensor llega al máximo de riegos del día.
- Cuando se enciende el sistema (por ejemplo, después de un corte de luz).
- Resumen diario a una hora configurable (por defecto 21:00): ml de cada maceta, humedad actual si usa sensor y si hay pausa.

Incluye un texto con los pasos para crear el bot (@BotFather, `/newbot`, pegar el token, escribirle al bot, autorizar el ID) y un botón "Enviar mensaje de prueba".

### Comandos

| Comando | Acción |
|---|---|
| `/estado` | Estado de cada maceta |
| `/regar 1`, `/regar kiwi`, `/regar todas` | Riego manual por número, parte del nombre o todas |
| `/detener` | Detiene el riego |
| `/pausa` y `/reanudar` | Pausa o reanuda el riego automático |
| `/horarios` | Horarios activos y macetas que riegan por sensor |
| `/ayuda` (también `/start`) | Lista de comandos |

Debe aceptar el formato de grupos (`/estado@NombreDelBot`).

---

## 8. Persistencia

Toda la configuración se guarda con `Preferences` en el espacio de nombres `riego` y sobrevive a cortes de luz: configuración de cada maceta, horarios, tiempo máximo, configuración de Telegram y estado de pausa. Los contadores diarios no se guardan.

---

## 9. Reglas que el código no debe romper

1. Nunca dos bombas encendidas al mismo tiempo.
2. Todas las bombas en LOW al arrancar, antes que cualquier otra cosa.
3. Ningún riego puede superar el tiempo máximo configurado.
4. El riego no se bloquea por la red: nada de esperas largas en el bucle principal.
5. Los sensores sin activar nunca disparan riegos.
6. El token de Telegram no sale nunca del ESP32 hacia la web.
7. Solo el chat autorizado puede dar órdenes.

---

## 10. Pendientes y próximos pasos

1. **Sensor de nivel del bidón** (GPIO 14): si no hay agua, bloquear todas las bombas, mostrarlo en la web y avisar por Telegram una sola vez hasta que se recargue.
2. **Piezas para imprimir en 3D**, idealmente en OpenSCAD con medidas como parámetros: caja para la electrónica, tapa del bidón con soporte para las tres bombas y pasajes para mangueras y cables, capuchón estanco para los sensores (cubre la zona de componentes y se rellena con silicona o epoxi), soporte de borde de maceta que fije el sensor a profundidad constante, estaca guía para abrir el agujero del sensor, anillo distribuidor para las hortensias (sujeta la manguera doblada en semicírculo con 4 a 6 agujeritos de 1,5 a 2 mm), estaca con difusor para el kiwi y clips de borde para las mangueras. Faltan las medidas reales: diámetro exterior de la manguera, espesor del borde de cada maceta y dimensiones de la placa del sensor.
3. **Opcionales:** validar el certificado de Telegram en lugar de `setInsecure()`; actualización del firmware por WiFi (OTA), útil cuando el ESP32 esté dentro de la caja.

---

## 11. Indicaciones para generar el repositorio

- Partir de `riego_web.ino` como referencia de comportamiento, y separarlo en archivos con responsabilidades claras (por ejemplo: configuración y persistencia, riego, sensores, servidor web y API, página web en un archivo de encabezado propio, y Telegram).
- Mantenerlo **compatible con el Arduino IDE** (carpeta del sketch con el mismo nombre que el `.ino` principal). Si se agrega `platformio.ini`, que sea como alternativa.
- **Las credenciales del WiFi no deben quedar en el repositorio:** ponerlas en un `secrets.h` incluido en `.gitignore`, y versionar un `secrets.example.h` de ejemplo.
- Agregar un `README.md` en español con: lista de materiales, diagrama de conexiones, instalación del paquete del ESP32 y de ArduinoJson, cómo cargar las credenciales, cómo medir el caudal de cada bomba (regar 10 s en un vaso medidor y dividir por 10), cómo calibrar los sensores y cómo configurar el bot de Telegram.
- Si es posible, verificar que compile con `arduino-cli` para la placa `esp32:esp32:nodemcu-32s`.
