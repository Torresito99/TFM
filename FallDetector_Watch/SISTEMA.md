# 🚨 Fall Detector Watch - Documentación del Sistema

## 📋 Estructura del Proyecto

```
src/
├── main.cpp              → Punto de entrada principal
├── power_manager.h/cpp   → Gestión de energía y Deep Sleep
├── bma_handler.h/cpp     → Control del acelerómetro BMA423
├── fall_detector.h/cpp   → Algoritmo de detección de caídas
├── bma_handler.h         → (Ya existente)
└── bma_handler.cpp       → (Ya existente, mejorado)
```

---

## 🔋 Flujo de Funcionamiento

```
┌─────────────────────────────────────────────────────────┐
│ 1. SETUP INICIAL                                        │
│  - Inicializar TTGO Watch                              │
│  - Encender LDO3 y LDO4 (sensor siempre alimentado)    │
│  - Configurar BMA423 (detectar movimiento)             │
│  - Limpiar interrupciones pendientes                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ 2. DEEP SLEEP                                           │
│  - AP32 en hibernación (μA de consumo)                 │
│  - Pantalla apagada                                     │
│  - BMA423 activo, monitoreando movimiento              │
│  - Esperando interrupción externa (GPIO39)             │
└─────────────────────────────────────────────────────────┘
                          ↓
                  ┌───────────────┐
          MOVIMIENTO DETECTADO    │
                  └───────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ 3. DESPERTADOR (ext0 wakeup)                           │
│  - ESP32 se despierta de Deep Sleep                    │
│  - BMA423 envió pulso a GPIO39                         │
│  - Pantalla se enciende                                │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ 4. CONFIRMAR CAÍDA (Algoritmo)                         │
│  - Leer 10 muestras del acelerómetro                   │
│  - Calcular magnitud de aceleración                    │
│  - ¿Hay caída libre (< 2 m/s²)?                       │
│  - ¿Hay impacto (> 25 m/s²)?                          │
│  - SI ambas = CAÍDA CONFIRMADA                         │
│  - SOLO impacto = Falsa alarma (golpe normal)          │
└─────────────────────────────────────────────────────────┘
                          ↓
                    ┌─────────────────────┐
         ┌──────────│ ¿CAÍDA CONFIRMADA?  │──────────┐
         │          └─────────────────────┘          │
         │                                            │
        [SÍ]                                         [NO]
         │                                            │
         ↓                                            ↓
   "hola TFM"                    Mostrar "Movimiento Normal"
   Vibración 3x                  Vibración 1x
   Alerta enviada                Descartar
         │                                            │
         └────────────────────┬──────────────────────┘
                              ↓
             ┌──────────────────────────────┐
             │ 5. VOLVER A DORMIR           │
             │  - Limpiar interrupciones    │
             │  - Apagar pantalla           │
             │  - Deep Sleep nuevamente     │
             └──────────────────────────────┘
```

---

## 📁 Módulos Detallados

### 🔌 **PowerManager** (`power_manager.h/cpp`)
**Responsabilidad**: Gestión de energía y modos de sueño

```cpp
// Inicializar energía
PowerManager::init(ttgo);

// Mantener el sensor alimentado (necesario para wake-on-motion)
PowerManager::keepSensorPowered(ttgo);

// Obtener razón del despertador
esp_sleep_wakeup_cause_t reason = PowerManager::getWakeupReason();

// Entrar en Deep Sleep
PowerManager::goToDeepSleep(ttgo);  // No regresa contraseña
```

**Puntos clave:**
- ✅ LDO3 y LDO4 siempre encendidos (I2C y sensor)
- ✅ Limpia interrupciones pendientes antes de dormir
- ✅ Configura GPIO39 como pin de despertador

---

### 📊 **BMAHandler** (`bma_handler.h/cpp`)
**Responsabilidad**: Control de bajo nivel del acelerómetro

```cpp
// Configurar el BMA423
BMAHandler::setupAccelerometer(ttgo);

// Verificar que el sensor está online (Chip ID 0x13)
bool online = BMAHandler::isSensorOnline(ttgo);

// Limpiar registro de interrupciones (CRÍTICO)
BMAHandler::clearAccelerometerInterrupts(ttgo);
```

**Puntos clave:**
- ✅ Habilita la característica WAKEUP del BMA423
- ✅ Limpia el registro de interrupciones (sin esto no despierta)
- ✅ Valida que el sensor responde

---

### 🚨 **FallDetector** (`fall_detector.h/cpp`)
**Responsabilidad**: Algoritmo de detección inteligente de caídas

```cpp
// Configurar detección de movimiento
FallDetector::setupMotionDetection(ttgo);

// Confirmar si el movimiento es realmente una caída
bool is_fall = FallDetector::confirmFall(ttgo);

// Debug: leer aceleración actual
FallDetector::debugPrintAccel(ttgo);
```

**Algoritmo:**
```
┌──────────────────────────────────────────┐
│ Leer 10 muestras (500ms total)          │
│ Calcular: magnitud = √(x² + y² + z²)   │
│ Encontrar: min_accel, max_accel         │
└──────────────────────────────────────────┘
           ↓
    ┌──────────────────┐
    │ Es caída si:     │
    ├──────────────────┤
    │ min < 2.0 m/s²   │ ← Caída libre
    │ AND              │
    │ max > 25.0 m/s²  │ ← Impacto
    └──────────────────┘
```

**Umbrales (ajustables):**
- `FREE_FALL_THRESHOLD = 2.0 m/s²`  → Caída libre
- `IMPACT_THRESHOLD = 25.0 m/s²`    → Impacto de gravedad
- `SAMPLE_COUNT = 10`                → Muestras para confirmar

---

## 🎯 **main.cpp** - Orquestador
Coordina todos los módulos:

1. **`displayMessage()`** → Mostrar textos en pantalla
2. **`vibrationFeedback()`** → Hacer vibrar (feedback háptico)
3. **`handleMotionDetected()`** → Procesar despertador + confirmar caída
4. **`setup()`** → Inicializar todo y gestionar despertadores
5. **`loop()`** → Vacío (el reloj está durmiendo)

---

## 🧪 Pruebas Recomendadas

### Prueba 1: ¿Se conecta el reloj?
```
Espera a ver en Serial:
✓ Sensor online (Chip ID: 0x13)
```

### Prueba 2: ¿Detecta movimiento?
```
Agita el reloj lentamente
→ El reloj debe despertarse
→ Mostrar "MOVIMIENTO / Detectado..."
```

### Prueba 3: ¿Distingue caída de golpe?
```
Golpe fuerte sobre mesa:
→ MAX: ~30-50 m/s² | MIN: ~9-10 m/s²
→ Resultado: "Movimiento Normal" (solo impacto)

Soltar el reloj desde la mano (caída):
→ MAX: ~25-60 m/s² | MIN: ~0-2 m/s²  
→ Resultado: "CAÍDA CONFIRMADA" + "hola TFM"
```

---

## 🔧 Ajustes Posibles

### Aumentar sensibilidad (detectar movimientos pequeños)
En `fall_detector.cpp`:
```cpp
const float FallDetector::IMPACT_THRESHOLD = 15.0;  // ← Bajar de 25 a 15
```

### Disminuir sensibilidad (ignorar movimientos pequeños)
```cpp
const float FallDetector::IMPACT_THRESHOLD = 35.0;  // ← Subir a 35
```

### Aumentar tiempo de confirmación
```cpp
const int FallDetector::SAMPLE_COUNT = 20;  // ← De 10 a 20 muestras (1 segundo)
```

---

## 🐛 Debugging

Abre el Monitor Serial (115200 baud) para ver:

```
[PowerManager] Entrando en Deep Sleep...
[PowerManager] Interrupciones limpias...
[BMAHandler] ✓ BMA423 inicializado
[FallDetector] Configurando detección...

>>> MOVIMIENTO DETECTADO <<<
[FallDetector] Confirmando si es caída...
[FallDetector] Muestra 1: 42.12 m/s²
[FallDetector] Muestra 2: 38.95 m/s²
...
[FallDetector] Min: 0.45 | Max: 45.23 | Caída Libre: SÍ | Impacto: SÍ
[FallDetector] ⚠️  CAÍDA CONFIRMADA
```

---

## 🔌 Pines Utilizados

| Pin | Función | Estado |
|-----|---------|--------|
| 39  | BMA INT (despertador) | INPUT |
| 4   | Motor vibración | OUTPUT |
| SDA | I2C (datos) | I2C |
| SCL | I2C (reloj) | I2C |

---

## 📚 Referencias

- **TTGO Watch V3**: Basada en ESP32 + LilyGO Watch Library
- **BMA423**: Acelerómetro de 3 ejes, detección de eventos integrada
- **Deep Sleep**: Consumo < 10 µA (motor apagado)
- **Wake-on-Motion**: Sensor activo consumiendo < 50 µA

---

## ✅ Estado Actual

- [x] Deep Sleep configurado
- [x] Wake-on-Motion habilitado
- [x] Algoritmo de detección de caídas
- [x] Módulos separados y limpios
- [x] Serial logging para debugging
- [ ] Persistencia de datos (guardar estadísticas)
- [ ] Conectividad WiFi/BLE (futuro)
- [ ] Servidor backend (futuro)

---

**Última actualización**: Marzo 2026
