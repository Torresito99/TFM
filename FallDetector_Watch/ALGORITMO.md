# 🎯 Algoritmo de Detección de Caídas - Documentación Técnica

## 📌 Concepto Base

El algoritmo implementa una **detección en dos fases**:

```
FASE 1: Detectar movimiento          FASE 2: Confirmar caída
(BMA423 hardware)          →         (ESP32 lógica soft)

Pin 39 recibe pulso              Leer 10 muestras
Despierta ESP32                  Analizar aceleración
                                 Distinguir caída vs golpe
```

---

## 🧮 Matemática: Magnitud de Aceleración

El BMA423 proporciona aceleración en 3 ejes (x, y, z).

Para saber la **aceleración total**, calculamos:

$$\text{Magnitud} = \sqrt{x^2 + y^2 + z^2}$$

### Ejemplos Reales:

| Situación | x | y | z | Magnitud | Clasificación |
|-----------|---|---|---|----------|---------------|
| Reposo | 0 | 0 | 9.8 | 9.8 m/s² | Normal |
| Caída libre | 0.1 | 0.1 | 0.1 | ~0.17 m/s² | ⚠️ CAÍDA |
| Caída + impacto | 12 | 15 | 30 | ~35 m/s² | 🚨 CRÍTICO |
| Golpe en mesa | 25 | 20 | 10 | ~33 m/s² | ⚠️ Falsa alarma |

---

## 🎬 Fases de una Caída Real

### Fase 1: Caída Libre (0-300ms)
- **Aceleración**: ~0.5-2.0 m/s²
- **Razón**: Sin contacto, solo gravedad residual
- **Detección**: `min_accel < 2.0`

### Fase 2: Impacto (300-500ms)
- **Aceleración**: ~25-60 m/s²
- **Razón**: Desaceleración bruzca al tocar el suelo
- **Detección**: `max_accel > 25.0`

### Fase 3: Rebote (500ms+)
- **Aceleración**: Decrece lentamente
- **No se procesa** (ya tomamos decisión)

---

## 🔍 Diagrama del Algoritmo

```
┌─────────────────────────────────────────────┐
│ PASO 1: Despertar                           │
│ - BMA423 genera pulso en GPIO39             │
│ - ESP32 sale de Deep Sleep                  │
│ - Logo "Movimiento Detectado..."            │
└─────────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────────┐
│ PASO 2: Leer 10 Muestras (500ms)           │
│ for i = 0 to 9:                             │
│   acc = leer_acelerometro()                 │
│   mag = sqrt(x² + y² + z²)                 │
│   guardar max_accel, min_accel             │
│   esperar 50ms                              │
│                                             │
│ Resultado:                                  │
│ max_accel = 38.45 m/s²                     │
│ min_accel = 0.67 m/s²                      │
└─────────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────────┐
│ PASO 3: Análisis                            │
│                                             │
│ ¿min_accel < 2.0?    → SÍ (0.67 < 2.0)    │
│   Resultado: CAÍDA_LIBRE = true             │
│                                             │
│ ¿max_accel > 25.0?   → SÍ (38.45 > 25.0)  │
│   Resultado: IMPACTO = true                 │
└─────────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────────┐
│ PASO 4: Decisión Final                      │
│                                             │
│ if (CAÍDA_LIBRE && IMPACTO):               │
│     return CAÍDA_CONFIRMADA ✅              │
│ else:                                       │
│     return FALSA_ALARMA ❌                  │
└─────────────────────────────────────────────┘
            ↓
  ┌─────────────────────────────────────┐
  │ if (CAÍDA_CONFIRMADA):              │
  │   - Mostrar "hola TFM"              │
  │   - Vibración 3x                    │
  │   - Guardar en log                  │
  │                                     │
  │ else:                               │
  │   - Mostrar "Movimiento Normal"     │
  │   - Vibración 1x                    │
  │                                     │
  │ Volver a Deep Sleep                 │
  └─────────────────────────────────────┘
```

---

## 📊 Matriz de Decisión

| min_accel | max_accel | Caída Libre | Impacto | Resultado |
|-----------|-----------|------------|---------|-----------|
| 0.5 | 30 | ✓ SÍ | ✓ SÍ | **🚨 CAÍDA** |
| 0.3 | 15 | ✓ SÍ | ✗ NO | ❌ Falsa alarma |
| 9.5 | 40 | ✗ NO | ✓ SÍ | ❌ Golpe normal |
| 9.0 | 12 | ✗ NO | ✗ NO | ❌ Movimiento normal |

---

## 🎚️ Parámetros Ajustables

### 1. FREE_FALL_THRESHOLD
```cpp
const float FallDetector::FREE_FALL_THRESHOLD = 2.0;
```

- **Significa**: Aceleración < 2.0 m/s² se considera caída libre
- **Aumentar** (ej: 3.0) → Menos sensible (ignora caídas suaves)
- **Disminuir** (ej: 1.0) → Más sensible (detecto caídas leves)

**Recomendación**: 1.5 - 2.5 m/s²

### 2. IMPACT_THRESHOLD
```cpp
const float FallDetector::IMPACT_THRESHOLD = 25.0;
```

- **Significa**: Aceleración > 25.0 m/s² se considera impacto
- **Aumentar** (ej: 35.0) → Menos sensible (ignora golpes blandos)
- **Disminuir** (ej: 15.0) → Más sensible (detecto caídas blandas)

**Recomendación**: 20 - 30 m/s²

### 3. SAMPLE_COUNT
```cpp
const int FallDetector::SAMPLE_COUNT = 10;
```

- **Significa**: Leer 10 muestras = 10 × 50ms = 500ms total
- **Aumentar** (ej: 20) → Más tiempo = más preciso pero lento
- **Disminuir** (ej: 5) → Menos tiempo = rápido pero menos preciso

**Recomendación**: 8 - 15 muestras

---

## 📈 Calibración Práctica

### Escenario 1: Demasiadas falsas alarmas

**Síntomas:**
- Cada vez que agito el reloj, dispara alarma
- "hola TFM" aparece por movimientos normales

**Solución:**
```cpp
// Aumentar umbrales
const float FallDetector::FREE_FALL_THRESHOLD = 3.0;  // ← ↑
const float FallDetector::IMPACT_THRESHOLD = 35.0;    // ← ↑
const int FallDetector::SAMPLE_COUNT = 15;            // ← ↑
```

**Recompila y prueba**

---

### Escenario 2: No detecta caídas reales

**Síntomas:**
- Suelto el reloj desde 30cm y no dispara
- Monitor serial: "Min: 3.5 | Max: 20 | Falsa alarma"

**Solución:**
```cpp
// Disminuir umbrales
const float FallDetector::FREE_FALL_THRESHOLD = 1.5;  // ← ↓
const float FallDetector::IMPACT_THRESHOLD = 20.0;    // ← ↓
const int FallDetector::SAMPLE_COUNT = 8;             // ← ↓
```

---

### Escenario 3: Muy lento en responder

**Síntomas:**
- Tarda >1 segundo en mostrar "hola TFM"
- Motor vibra lentamente

**Solución:**
```cpp
// Disminuir muestras
const int FallDetector::SAMPLE_COUNT = 5;  // ← De 10 a 5 (250ms)
```

---

## 🧪 Testeo Manual

### Test 1: Verificar lecturas

```cpp
// En confirmFall(), antes del return:
FallDetector::debugPrintAccel(ttgo);
```

Sueltas el reloj → Llevan datos en Serial:
```
[DEBUG] X: 15.23 | Y: -8.45 | Z: 28.67 | Mag: 32.10 m/s²
```

---

### Test 2: Offline simulation

Sin tocar el reloj:
```
Aceleración = ~9.8 m/s² (solo gravedad) → Falsa alarma ✓
```

---

### Test 3: Caída sobre cama

Suelta desde 1m sobre colchón:
```
[DEBUG] Min: 0.2 m/s² | Max: 40 m/s²
[FallDetector] ⚠️ CAÍDA CONFIRMADA ✓
```

---

## 🔬 Física Detrás

### ¿Por qué funciona este método?

**Caída libre:**
- Solo actúa gravedad
- a ≈ 0 (aceleración nula una vez conocida)
- Sensor detecta ~0.5-2 m/s²

**Impacto:**
- Al tocar el suelo, desaceleración bruzca
- F = ma → Aceleración inversa grande
- Sensor detecta > 25 m/s²

**Golpe normal (mesa):**
- Aceleración sin caída previa
- Solo impacto, sin fase de caída libre
- No pasa ambas condiciones → Se ignora

---

## 📋 Checklist de Calibración

- [ ] Sistema compila sin errores
- [ ] Serial muestra "Sensor online"
- [ ] Agitar → Despierta y muestra "Movimiento Detectado"
- [ ] Golpe fuerte → Muestra "Movimiento Normal" (NO es caída)
- [ ] Soltar desde 30cm → Muestra "hola TFM" (ES caída)
- [ ] Ajustar umbrales según necesidad
- [ ] Documentar valores finales en `fall_detector.cpp`

---

## 🎯 Ejemplo de Calibración Final

```cpp
// CONFIGURACIÓN FINAL PARA USUARIO:
// - Detecta caídas desde ~20cm
// - Ignora golpes normales
// - Respuesta en <500ms

const float FallDetector::FREE_FALL_THRESHOLD = 2.0;  // ✓
const float FallDetector::IMPACT_THRESHOLD = 25.0;    // ✓
const int FallDetector::SAMPLE_COUNT = 10;             // ✓
```

---

**Última actualización**: Marzo 2026
