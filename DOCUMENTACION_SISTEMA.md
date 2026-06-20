# Sistema de Detección de Caídas y Alarma Ambiental — Documentación Técnica (TFM)

> **Nota de seguridad:** en este documento **todas las contraseñas, claves API,
> tokens, números de teléfono y credenciales** se han sustituido por
> marcadores tipo `<...>`. No contiene ningún dato confidencial. Las
> direcciones IP que aparecen son de **red local privada** (no enrutables a
> Internet) y se incluyen solo para explicar las conexiones.

---

## 1. Visión general

El sistema detecta **caídas** de una persona mayor (mediante un reloj en la
muñeca) y, además, vigila el **ambiente** (temperatura, humedad, gases). Cuando
se confirma una caída, el aviso se propaga por varios canales hasta acabar en
una **llamada de voz** a los cuidadores y notificaciones por mensajería.

La filosofía del diseño es **desacoplar** cada función en un dispositivo o
servicio especializado, comunicados por protocolos estándar (BLE, MQTT, SIP,
AMI). Así cada pieza es sencilla, sustituible y fácil de depurar.

### Diagrama del flujo completo

```
 ┌─────────────────────┐   BLE advertising      ┌──────────────────────────┐
 │ FallDetector Watch   │ ── "TFM-FALL" ───────▶ │  C02 Alarm Detector       │
 │ (New Version, ESP32-C6)│   (sin conexión)      │  (Gateway, ESP32 Mini)    │
 │  - Detecta la caída  │                        │  - Escucha la baliza BLE  │
 │  - Emite baliza BLE  │                        │  - Lee BME680 (ambiente)  │
 └─────────────────────┘                         └────────────┬─────────────┘
                                                               │ MQTT publish
                                                               │ topic: falldetector/fall
                                                               ▼
                                                  ┌──────────────────────────┐
                                                  │  Mosquitto (broker MQTT)  │
                                                  │  add-on de Home Assistant │
                                                  └────────────┬─────────────┘
                                                               │ MQTT subscribe
                                                               ▼
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                          HOME ASSISTANT (Raspberry Pi)                      │
 │  • Automatización: al recibir "falldetector/fall" → espera 2 s →           │
 │    ejecuta shell_command "asterisk_call_fall"                              │
 │  • shell_command: abre TCP al AMI de Asterisk y manda Originate            │
 │  • notify: avisos por WhatsApp / Telegram (CallMeBot, REST)                │
 └───────────────┬───────────────────────────────────────┬──────────────────┘
                 │ AMI (TCP 5038)                          │ HTTPS REST
                 ▼                                         ▼
 ┌──────────────────────────┐                   ┌──────────────────────────┐
 │   Asterisk (add-on HA)    │                   │   CallMeBot (WhatsApp/    │
 │   • Endpoints PJSIP       │                   │   Telegram, en la nube)   │
 │   • Dialplan (extensión   │                   └──────────────────────────┘
 │     201 = mensaje de voz) │
 └────────────┬─────────────┘
              │ SIP (señalización) + RTP (audio)
              │     ──── a través de la VPN Tailscale ────
              ▼
 ┌──────────────────────────┐
 │  Linphone (móviles)       │
 │  2000 (Android), 2001..   │
 │  (cuidadores/familiares)  │
 └──────────────────────────┘
```

---

## 2. Componentes (cada parte por separado)

### 2.1. FallDetector Watch — *New Version*

**Carpeta:** `FallDetector_Watch_NewVersion/`
**Hardware:** reloj **Waveshare ESP32-C6 2.06" Touch AMOLED** (pantalla AMOLED
CO5300, táctil FT3168, IMU, códec de audio ES8311, PMU/batería). Se lleva en la
muñeca.

**Función:** detectar caídas y, al confirmarse, **emitir una baliza BLE**. Es la
única responsabilidad del reloj en esta versión: **no** habla con Home Assistant
ni con la red WiFi. Solo "grita" por Bluetooth que ha habido una caída.

**Algoritmo de detección (3 fases obligatorias + puntuación de confianza):**
1. **Movimiento previo** — el brazo se mueve/cae (giro o aceleración dinámica
   por encima de un umbral).
2. **Impacto** — pico brusco de aceleración (magnitud alta + jerk alto o giro
   alto).
3. **Inmovilidad** — tras el impacto, la persona se queda quieta un tiempo.

Se exige que ocurran las tres en secuencia; además se acumula una **puntuación**
(impacto fuerte, jerk fuerte, giro fuerte, cambio de orientación de "de pie" a
"tumbado", etc.) y solo se confirma la caída si supera un umbral. Esto reduce
los falsos positivos.

**Comportamiento al detectar:**
- Arranca una **cuenta atrás de pre-alarma** (cancelable desde la pantalla):
  permite anular una falsa alarma.
- Si no se cancela, **suena la alarma** y comienza la **emisión BLE**.
- Una vez sonando, la alarma **solo se detiene con el botón BOOT** (no con la
  pantalla), para evitar cancelaciones accidentales.

**La baliza BLE (el "contrato" con el resto del sistema):**
- Tipo: *advertising* BLE (anuncio sin conexión; cualquiera puede escucharlo).
- Nombre del dispositivo: **`TFM-FALL`**.
- Datos de fabricante: bytes `{0xFF, 0xFF, 'F', 'A', 'L', 'L'}` (contienen la
  cadena `FALL`).
- Al cancelar la alarma, la emisión se detiene y se apaga la radio BLE (ahorro).

**Módulos del firmware:** `fall_detector` (algoritmo), `ble_alert` (baliza),
`alarm_audio` (sonido), `ui` (pantalla), `batt_log` (registro de batería),
`config.h` (pines y umbrales). Incluye ahorro de energía (light sleep, bajada de
frecuencia de CPU) para durar más con batería.

---

### 2.2. C02 Alarm Detector — *Gateway de caídas + sensor ambiental*

**Carpeta:** `C02_Alarm_Detector/`
**Hardware:** **ESP32 Mini** (módulo ESP32-WROOM-32, chip USB **CH9102**) +
sensor **BME680** por I2C. Es un dispositivo **fijo y enchufado** (no va con
batería), lo que le permite tener WiFi y escuchar BLE permanentemente.

**Doble función:**

**A) Gateway BLE → MQTT (la parte crítica):**
- Escanea **continuamente** por BLE buscando la baliza `TFM-FALL` del reloj
  (escaneo no bloqueante que se reanuda solo).
- Detección **por flanco**: cuando la baliza **aparece** (tras estar ausente),
  publica **una vez** el aviso. Mientras la baliza siga presente no repite el
  envío (evita spam). Si la baliza desaparece > 4 s, se considera finalizada y
  el sistema queda **rearmado** para la siguiente caída.
- Al detectar la caída, **publica por MQTT** exactamente la misma señal que
  usaba el sistema original:
  - **Topic:** `falldetector/fall`
  - **Payload:** `{"event":"fall_detected"}`
  - **Retained:** sí
- Se conecta al broker MQTT en `192.168.1.181:1883` (WiFi + librería
  PubSubClient). La conexión WiFi/MQTT es **no bloqueante**: si no hay red,
  reintenta en segundo plano y el evento queda pendiente hasta poder enviarse.

**B) Lectura ambiental (BME680):**
- Mide **temperatura, humedad, presión y resistencia de gas**, y calcula un
  índice de calidad de aire (IAQ) y un CO2 estimado (eCO2).
- Evalúa **umbrales adaptados a personas mayores** (calor, frío, humedad,
  gases) y, de momento, **muestra todo por el monitor serie** (no se envía a HA
  todavía).
- Si el BME680 no está conectado, **no es fatal**: el gateway avisa y sigue
  funcionando la parte de caídas (que es la prioritaria).

**Pines / build:** I2C en `SDA=GPIO21`, `SCL=GPIO22`. Proyecto PlatformIO,
entorno `esp32-mini` (placa `esp32dev`, partición `huge_app.csv` para que quepan
BLE + WiFi + MQTT). Módulos: `ble_listener`, `ha_notify`, `main` (BME680 +
orquestación), `config.h`, `alarms.h`, `secrets.h` (credenciales, **no se sube
al repositorio**).

---

### 2.3. Mosquitto — *Broker MQTT*

**Qué es:** el "buzón" central de mensajes. Funciona como **add-on de Home
Assistant** en la Raspberry Pi, escuchando en el puerto **1883**.

**Su papel:** recibe los mensajes que publica el gateway C02 (topic
`falldetector/fall`) y los entrega a quien esté suscrito (Home Assistant). Es un
**desacoplador**: el que publica y el que escucha no necesitan conocerse, solo
comparten el nombre del topic.

**Acceso:** usuario/contraseña MQTT `<usuario_mqtt>` / `<contraseña_mqtt>`
(definidos en el broker; en el gateway viven en `secrets.h`).

---

### 2.4. Home Assistant — *Cerebro/orquestador*

**Dónde:** Raspberry Pi, IP `192.168.1.181`. Es el centro que reacciona a los
eventos y dispara las acciones. Configuración en `configuration.yaml` y
`automations.yaml`.

**Piezas relevantes:**

- **Sensores MQTT** (en `configuration.yaml`): exponen como entidades la
  información de batería que llega por el topic `falldetector/battery`
  (porcentaje, voltaje, estado de carga). *(Topic heredado del sistema previo;
  útil si en el futuro se vuelve a publicar batería.)*

- **Automatización (la clave del aviso):**
  1. **Disparador:** se recibe un mensaje MQTT en `falldetector/fall`.
  2. **Espera** 2 segundos (estabilización).
  3. **Acción:** ejecuta el servicio `shell_command.asterisk_call_fall`.

- **`shell_command.asterisk_call_fall`:** un pequeño script en Python (embebido
  en `configuration.yaml`, sin archivos externos) que:
  - Abre una conexión **TCP al AMI de Asterisk** (`192.168.1.181:5038`).
  - Hace **Login** y manda una o varias acciones **Originate** (con
    `Async: yes`) para que Asterisk **llame a los móviles** y les ponga el
    mensaje grabado.
  - Las credenciales del AMI (`<usuario_AMI>` / `<secreto_AMI>`) van en ese
    script. *(Detalle aprendido: `shell_command` de HA no usa una shell, por eso
    no admite pipes `|`; por eso se usa `python3 -c`, que abre el socket
    directamente.)*

- **Notificaciones de mensajería (`notify` y `rest_command`):** mediante
  **CallMeBot** (servicio en la nube, por REST/HTTPS) se mandan avisos de
  **WhatsApp** y **Telegram**. Datos sensibles redactados:
  `<numero_whatsapp>`, `<apikey_callmebot>`, `<usuario_telegram>`.

- **`input_number`:** entidades preparadas para mostrar temperatura/humedad del
  ESP32 (para una futura integración del BME680 con HA).

---

### 2.5. Asterisk — *Centralita de voz (PBX)*

**Dónde:** add-on de Home Assistant en la Raspberry (mismo equipo,
`192.168.1.181`). Configuración en `/addon_configs/<slug>_asterisk/asterisk/`
(carpetas `custom/` y `default/`).

**Su papel:** convertir el evento "ha habido una caída" en **llamadas de
teléfono** a los cuidadores, reproduciendo un mensaje grabado.

**Bloques de configuración:**

- **`pjsip_custom` (endpoints SIP):** define los "teléfonos" `2000`, `2001`,
  `2002`… Cada extensión tiene tres objetos:
  - `endpoint` — códecs (`ulaw`, `alaw`), opciones de NAT
    (`rtp_symmetric=yes`, `force_rport=yes`, `rewrite_contact=yes`,
    `direct_media=no`).
  - `auth` — usuario y contraseña SIP (`<contraseña_SIP>`, redactada).
  - `aor` — dónde se registra el dispositivo (`max_contacts`,
    `qualify_frequency` para mantener vivo el registro y detectar caídas de
    conexión).

- **`manager` (AMI):** interfaz de administración por TCP en el puerto **5038**,
  usada por Home Assistant para ordenar las llamadas. Restringida por IP a la
  red local. Usuario `<usuario_AMI>` / secreto `<secreto_AMI>`.

- **`extensions.conf` (dialplan, contexto `default`):**
  - **Extensión `201`** — el "mensaje de aviso": contesta la llamada
    (`Answer`), reproduce el audio `alertacaida.wav` **3 veces** (bucle `While`)
    con pausas, y cuelga. La ruta del audio es
    `/config/asterisk/custom/alertacaida` (dentro del add-on).
  - **Disparo de llamadas** — Home Assistant, vía AMI, hace `Originate` hacia
    cada extensión enviándolas a la `201`, de modo que el móvil suena y oye el
    mensaje.

**Dos modos posibles de llamar a varios:**
- **Simultáneo/independiente:** varios `Originate` por AMI con `Async: yes` →
  todos los móviles suenan a la vez, cada uno reproduce el mensaje por su cuenta.
- **Cascada/escalado:** con `Dial(PJSIP/2000,T)` en líneas sucesivas → llama al
  primero; si no contesta en `T` segundos, al siguiente; y así. El **orden** lo
  marca el orden de las líneas y el **salto** lo marca el *timeout* de cada uno.

---

### 2.6. Tailscale — *VPN para acceso remoto seguro*

**Qué es:** una VPN tipo malla (basada en WireGuard) que mete a los dispositivos
en una **red privada cifrada**, estén donde estén. Se ejecuta como **add-on de
Home Assistant** (la Raspberry obtiene una IP de Tailscale tipo `100.x.x.x`).

**Por qué se necesita:** para que los móviles de los cuidadores reciban la
llamada **también fuera de casa** (datos móviles). Sin VPN, llegar a Asterisk
desde Internet exigiría abrir puertos (inseguro y a menudo imposible por el
CGNAT del operador). Con Tailscale, el móvil "está dentro de la red de la Pi"
desde cualquier sitio, sin abrir puertos.

**Requisito:** Tailscale debe estar instalado y conectado **en cada móvil** que
deba recibir la llamada en remoto (es el otro extremo de la VPN).

---

### 2.7. Linphone — *Softphone SIP en los móviles*

**Qué es:** la app de teléfono SIP instalada en los móviles de los cuidadores.
Se **registra** contra Asterisk (extensiones `2000`, `2001`, …) y es la que
**suena** y reproduce el mensaje cuando hay una caída.

**Configuración de cuenta (sin datos sensibles):**
- Usuario: número de extensión (`2000`, `2001`…).
- Contraseña: `<contraseña_SIP>` (la del `auth` correspondiente en Asterisk).
- Dominio/Proxy: la **IP de Tailscale** de la Pi (para que funcione dentro y
  fuera) o la IP local `192.168.1.181` (solo en casa).
- Transporte: UDP.
- Ajustes recomendados: *keep-alive* activado, *expire* ~120 s, y en el sistema
  operativo **batería sin restricciones** + *autostart* para que no se cierre.

**Aviso por plataforma:**
- **Android:** con los ajustes anteriores suele mantener el registro.
- **iPhone (iOS):** iOS mata las apps en segundo plano, así que aunque registre,
  puede no entrar la llamada con la app cerrada. La solución robusta es un
  cliente con **push** (Groundwire/Zoiper) sobre esta misma VPN.

---

## 3. Cómo interactúa cada cosa con cada cosa

| Origen → Destino | Protocolo / Puerto | Qué viaja |
|---|---|---|
| Reloj → Gateway C02 | **BLE advertising** (sin conexión) | Baliza `TFM-FALL` (señal de caída) |
| Gateway C02 → Mosquitto | **MQTT** sobre TCP **1883** (WiFi) | `falldetector/fall` = `{"event":"fall_detected"}` |
| Mosquitto → Home Assistant | **MQTT** (suscripción) | Entrega del mismo mensaje |
| Home Assistant → Asterisk | **AMI** sobre TCP **5038** | Acción `Originate` (ordenar llamadas) |
| Asterisk → Linphone | **SIP** (señalización) + **RTP** (audio), UDP **5060** | La llamada y el mensaje de voz |
| Linphone ↔ Asterisk (remoto) | **Tailscale (WireGuard)** | Túnel cifrado que transporta el SIP/RTP |
| Home Assistant → CallMeBot | **HTTPS/REST** | Texto del aviso a WhatsApp/Telegram |

**Puntos clave de las interacciones:**

- El reloj y el gateway están **desacoplados por BLE**: el reloj no sabe quién le
  escucha; el gateway no necesita emparejarse, solo escanear.
- El gateway y Home Assistant están **desacoplados por MQTT**: el "contrato" es
  únicamente el topic `falldetector/fall` y su payload. Cualquier dispositivo
  que publique ese mensaje dispara el mismo aviso (el reloj original lo hacía
  directamente; ahora lo hace el gateway tras "traducir" la baliza BLE).
- Home Assistant es el **único** que orquesta acciones: decide qué hacer cuando
  llega el evento (llamar por Asterisk, avisar por WhatsApp/Telegram, etc.).
- Asterisk y Home Assistant conviven en la **misma Raspberry**, por eso el AMI
  es accesible en `192.168.1.181:5038`.
- Tailscale es **transparente**: SIP/RTP viajan igual, solo que por un túnel
  cifrado, lo que permite el uso remoto sin exponer Asterisk a Internet.

---

## 4. Flujo completo, paso a paso

1. La persona **cae**. El reloj detecta la secuencia (movimiento → impacto →
   inmovilidad) y, si supera la puntuación de confianza, inicia la **pre-alarma**
   (cancelable).
2. Si no se cancela, el reloj **suena** y empieza a **emitir la baliza BLE**
   `TFM-FALL`.
3. El **gateway C02**, que escanea sin parar, **detecta la baliza** (flanco de
   subida) y **publica** por MQTT `falldetector/fall` → `{"event":"fall_detected"}`
   (retenido) en **Mosquitto**.
4. **Home Assistant**, suscrito a ese topic, dispara su **automatización**:
   espera 2 s y llama a `shell_command.asterisk_call_fall`.
5. El **shell_command** abre el **AMI** de Asterisk y manda los `Originate`.
6. **Asterisk** llama a los móviles (`2000`, `2001`, …) y, al descolgar, los
   envía a la extensión **`201`**, que reproduce el **mensaje grabado**.
7. La llamada llega a **Linphone** en los móviles, a través de **Tailscale** si
   están fuera de casa.
8. En paralelo, Home Assistant puede enviar **avisos por WhatsApp/Telegram**
   (CallMeBot).
9. Cuando la alarma del reloj se **cancela**, la baliza desaparece; el gateway lo
   detecta y queda **rearmado** para la siguiente caída.

---

## 5. Dónde viven las credenciales (todas redactadas)

| Dato | Dónde está | En este documento |
|---|---|---|
| SSID/clave WiFi | `C02_Alarm_Detector/include/secrets.h` | `<WIFI_SSID>` / `<contraseña_wifi>` |
| Usuario/clave MQTT | `secrets.h` + broker Mosquitto | `<usuario_mqtt>` / `<contraseña_mqtt>` |
| Contraseñas SIP (2000, 2001…) | `pjsip_custom` (Asterisk) | `<contraseña_SIP>` |
| Usuario/secreto AMI | `manager` (Asterisk) + `shell_command` (HA) | `<usuario_AMI>` / `<secreto_AMI>` |
| Teléfono/clave CallMeBot | `configuration.yaml` (HA) | `<numero_whatsapp>` / `<apikey_callmebot>` |
| Usuario Telegram | `configuration.yaml` (HA) | `<usuario_telegram>` |

> Recomendación: mantener `secrets.h` fuera del control de versiones (ya está en
> `.gitignore`) y, en Home Assistant, usar `secrets.yaml` para no exponer claves
> en `configuration.yaml`.

---

## 6. Resumen de red (IPs y puertos locales)

| Elemento | Dirección | Puerto |
|---|---|---|
| Raspberry Pi (Home Assistant + Mosquitto + Asterisk + Tailscale) | `192.168.1.181` | — |
| Broker MQTT (Mosquitto) | `192.168.1.181` | `1883` |
| AMI de Asterisk | `192.168.1.181` | `5038` |
| SIP de Asterisk | `192.168.1.181` (o IP Tailscale `100.x.x.x` en remoto) | `5060` (UDP) |
| Gateway C02 (ESP32 Mini) | IP por DHCP (ej. `192.168.1.172`) | — |

---

## 7. Estado actual y trabajo futuro

**Funciona hoy:**
- Detección de caída en el reloj y emisión de baliza BLE.
- Gateway C02: relé BLE → MQTT y lectura del BME680 por serie.
- HA recibe el evento y, por AMI, hace que Asterisk **llame y reproduzca el
  mensaje**.
- Cliente Linphone (Android, ext. `2000`) registrado y recibiendo la llamada,
  incluso en remoto vía Tailscale.

**Pendiente / mejorable:**
- Registrar más móviles (`2001`, `2002`) — endpoints ya preparados.
- Decidir modo de llamada definitivo (simultáneo vs cascada).
- En iPhone, valorar cliente con **push** por las limitaciones de iOS.
- Publicar los datos del **BME680** a Home Assistant (ahora solo van por serie).
- Cablear el BME680 al gateway si aún no lo está.
```
