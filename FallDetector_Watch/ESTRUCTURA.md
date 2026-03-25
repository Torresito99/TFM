# 📊 Estructura del Proyecto - Vista Completa

## 📁 Árbol del Proyecto

```
FallDetector_Watch/
│
├── 📚 DOCUMENTACIÓN
│   ├── INICIO_RAPIDO.md       ⚡ EMPIEZA AQUÍ (5 minutos)
│   ├── TESTING.md              🧪 Guía de compilación y pruebas
│   ├── ALGORITMO.md            🎯 Documentación técnica del algoritmo
│   ├── SISTEMA.md              📖 Descripción completa del sistema
│   ├── RESUMEN.md              📋 Resumen de cambios
│   └── README.md               ℹ️ Original del proyecto
│
├── 🛠️ CÓDIGO
│   ├── src/
│   │   ├── ⭐ main.cpp         → Programa principal (REFACTORIZADO)
│   │   ├── ⭐ power_manager.h/cpp     → Gestión de energía (NUEVO)
│   │   ├── ⭐ fall_detector.h/cpp     → Algoritmo de detección (NUEVO)
│   │   ├── ✏️  bma_handler.h/cpp      → Control del sensor (MEJORADO)
│   │
│   ├── include/
│   ├── lib/
│   ├── test/
│   └── platformio.ini  → Configuración del proyecto
│
└── ⚙️ CONFIGURACIÓN
    ├── .gitignore
    ├── .vscode/
    └── .pio/
```

---

## 🎯 Relación entre Módulos

```
                    ┌──────────────────┐
                    │    main.cpp      │
                    │  (Orquestador)   │
                    └──────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
    │PowerManager  │ │ BMAHandler   │ │FallDetector  │
    │              │ │              │ │              │
    │ -init()      │ │-setup()      │ │-setup()      │
    │ -sleep()     │ │-clear()      │ │-confirm()    │
    │ -keepOn()    │ │-online()     │ │-analyze()    │
    └──────────────┘ └──────────────┘ └──────────────┘
           │               │               │
    ┌──────────────────────────────────────────┐
    │     TTGO Watch V3 + BMA423 Sensor        │
    └──────────────────────────────────────────┘
```

---

## 📝 Descripción de Cada Módulo

### 🎬 **main.cpp** - Punto de Entrada Principal
**Responsabilidad:** Orquestar todo el sistema

**Funciones Principales:**
- `setup()` → Inicializa todo y gestiona despertadores
- `handleMotionDetected()` → Procesa evento de movimiento
- `displayMessage()` → Mostrar texto en pantalla
- `vibrationFeedback()` → Hacer vibrar el reloj
- `loop()` → Vacío (reloj está durmiendo)

**Flujo:**
```
setup() → Inicializar → Detectar causa despertador → 
  Si movimiento: handleMotionDetected() → 
    Si caída: Mostrar "hola TFM" → 
  Volver a dormir
```

---

### ⚡ **PowerManager** - Gestión de Energía
**Responsabilidad:** Deep Sleep y control de alimentación

**Archivos:** `power_manager.h` + `power_manager.cpp`

**Funciones Clave:**
```cpp
PowerManager::init(ttgo)                    // Initializar LDO3/LDO4
PowerManager::keepSensorPowered(ttgo)       // Mantener sensor on
PowerManager::goToDeepSleep(ttgo)           // Dormir profundo
PowerManager::getWakeupReason()             // ¿Por qué despertó?
```

**Consumo Energético:**
- Deep Sleep: ~50 µA
- Despierto: ~80-150 mA
- Con pantalla: +200-300 mA

---

### 🔌 **BMAHandler** - Control del Acelerómetro
**Responsabilidad:** Interfaz low-level con BMA423

**Archivos:** `bma_handler.h` + `bma_handler.cpp`

**Funciones Clave:**
```cpp
BMAHandler::setupAccelerometer(ttgo)        // Configurar sensor
BMAHandler::clearAccelerometerInterrupts()  // CRÍTICO: limpiar estado
BMAHandler::isSensorOnline(ttgo)            // Validar chip
```

**Características Importantes:**
- Chip ID validación: `0x13` = OK
- Limpieza de interrupciones pendientes
- Logging etiquetado con `[BMAHandler]`
- Uso de namespace para organización

---

### 🚨 **FallDetector** - Algoritmo de Detección
**Responsabilidad:** Detectar y confirmar caídas

**Archivos:** `fall_detector.h` + `fall_detector.cpp`

**Funciones Clave:**
```cpp
FallDetector::setupMotionDetection(ttgo)    // Activar detección
FallDetector::confirmFall(ttgo)             // Leer 10 muestras + confirmar
FallDetector::debugPrintAccel(ttgo)         // Debug de valores
```

**Parámetros Configurables:**
```cpp
FREE_FALL_THRESHOLD = 2.0 m/s²     // Umbral detección caída libre
IMPACT_THRESHOLD = 25.0 m/s²       // Umbral detección impacto
SAMPLE_COUNT = 10                   // Muestras a leer
```

**Lógica:**
```
Leer 10 muestras (500ms)
  ↓
Buscar min y max de aceleración
  ↓
¿min < 2.0? → Caída libre ✓
¿max > 25.0? → Impacto ✓
  ↓
Ambas = CAÍDA CONFIRMADA
Solo impacto = FALSA ALARMA
```

---

## 📊 Diagrama de Flujo General

```
┌─────────────────────────────────────┐
│  1. INICIO / DESPERTADOR            │
│  - Reloj enciende                   │
│  - Podría ser por botón o mov       │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│  2. INICIALIZACIÓN                  │
│  - PowerManager::init()             │
│  - BMAHandler::setupAccelerometer() │
│  - FallDetector::setup()            │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│  3. ¿QUÉ CAUSÓ DESPERTADOR?         │
│  esp_sleep_get_wakeup_cause()       │
└─────────────────────────────────────┘
           ↓
      ┌────┴────┐
      │         │
    BOTÓN    MOVIMIENTO (GPIO39)
      │         │
      ↓         ↓
  "Sistema    handleMotionDetected()
   Armado"      │
      │         FallDetector::confirmFall()
      │         │
      │         ┌─────────┬─────────┐
      │         │         │         │
      │      CAÍDA    FALSA ALARMA
      │         │         │
      │    "hola TFM"  "Movimiento"
      │    Vibra 3x    Vibra 1x
      │         │         │
      └─────────┴─────────┘
              ↓
┌─────────────────────────────────────┐
│  4. LIMPIAR Y DORMIR                │
│  - BMAHandler::clearInterrupts()    │
│  - PowerManager::goToDeepSleep()    │
└─────────────────────────────────────┘
           ↓
    [DURMIENDO 50µA]
    ← ESPERAR PRÓXIMO MOVIMIENTO
```

---

## 🔄 Ciclo de Vida del Dispositivo

### Ciclo Normal (Sin Caída)

```
┌─────────────────────────────────────────────────────────┐
│ t=0ms: DURMIENDO (Deep Sleep, 50µA)                     │
│        BMA423 activo, vigilando movimiento              │
└─────────────────────────────────────────────────────────┘
                        ↓ (t=2000ms)
┌─────────────────────────────────────────────────────────┐
│ t=2000ms: MOVIMIENTO NORMAL (ej: agitar)                │
│           GPIO39 recibe pulso                           │
│           ESP32 se despierta                            │
└─────────────────────────────────────────────────────────┘
                        ↓ (t=2100ms)
┌─────────────────────────────────────────────────────────┐
│ t=2100ms: CONFIRMACIÓN (leyendo 10 muestras)            │
│           Min: 8.5 m/s² | Max: 15.0 m/s²               │
│           Resultado: Falsa alarma (movimiento normal)   │
└─────────────────────────────────────────────────────────┘
                        ↓ (t=2600ms)
┌─────────────────────────────────────────────────────────┐
│ t=2600ms: MOSTRAR EN PANTALLA                           │
│           "Movimiento Normal" durante 2 segundos        │
│           Vibrar 1 vez                                  │
└─────────────────────────────────────────────────────────┘
                        ↓ (t=4600ms)
┌─────────────────────────────────────────────────────────┐
│ t=4600ms: LIMPIAR Y DORMIR                              │
│           Reset interrupciones                          │
│           Volver a Deep Sleep                           │
└─────────────────────────────────────────────────────────┘
                        ↓
│ [DURMIENDO NUEVAMENTE] ← Volver a vigilar
```

### Ciclo de Caída Confirmada

```
[DURMIENDO]
    ↓ (movimiento brusco)
[MOVIMIENTO DETECTADO] ← T=0ms
    ↓
[LEE 10 MUESTRAS] ← T=0-500ms
Min: 0.5 m/s² (caída libre)
Max: 38.0 m/s² (impacto)
    ↓
[AMBAS CONDICIONES MET]
    ↓
[MOSTRAR "hola TFM"] ← T=500-2500ms
[VIBRA 3 VECES]
    ↓
[VOLVER A DORMIR] ← T=2500ms
[DURMIENDO]
```

---

## 🎨 Relaciones y Dependencias

```
main.cpp
  ├─ include <Arduino.h>
  ├─ include <LilyGoWatch.h>
  ├─ include "power_manager.h"
  ├─ include "bma_handler.h"
  └─ include "fall_detector.h"

power_manager.cpp
  └─ include "power_manager.h"
  
bma_handler.cpp
  └─ include "bma_handler.h"
  
fall_detector.cpp
  ├─ include "fall_detector.h"
  ├─ include <Arduino.h>
  └─ include <LilyGoWatch.h>
```

---

## 📦 Archivos de Configuración

### platformio.ini
```ini
[env:ttgo-t-watch]
platform = espressif32
board = ttgo-t-watch
framework = arduino
monitor_speed = 115200
upload_speed = 921600
build_flags =
    -D LILYGO_WATCH_2020_V3
lib_deps =
    https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library.git
```

---

## 🎓 Mapa de Referencias Rápidas

| Necesito... | Archivo |
|------------|---------|
| **Empezar rápido** | INICIO_RAPIDO.md |
| **Compilar y probar** | TESTING.md |
| **Entender algoritmo** | ALGORITMO.md |
| **Entender arquitectura** | SISTEMA.md |
| **Ver todos cambios** | RESUMEN.md |
| **Modificar parámetros** | fall_detector.cpp (líneas 5-7) |
| **Agregar logging** | main.cpp (funciones con Serial.println) |
| **Configurar pinout** | power_manager.h (define BMA_INT_PIN) |

---

## ✨ Características Implementadas

✅ Wake-on-Motion (GPIO39 como despertador)
✅ Deep Sleep sin sensor (50µA consumo)
✅ Algoritmo inteligente de detección
✅ Módulos separados y reutilizables
✅ Logging detallado con etiquetas
✅ Parámetros configurable sin hardcoding
✅ Documentación técnica completa
✅ Casos de prueba definidos
✅ Tremofeedback (vibración)
✅ Interfaz visual (texto en pantalla)

---

## 🚀 Pronto...

🔜 Persistencia de datos (SPIFFS)
🔜 WiFi/BLE connectivity
🔜 Backend para cloud storage
🔜 App móvil para notificaciones
🔜 Machine Learning para calibración automática
🔜 Generación de reportes

---

**Estado del Proyecto:** ✅ COMPLETO Y LISTO PARA PROBAR

Creado: Marzo 2026
