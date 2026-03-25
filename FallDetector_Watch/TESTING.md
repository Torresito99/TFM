# ⚡ Guía Rápida de Compilación y Prueba

## 📦 Compilar el Proyecto

### Opción 1: Desde VS Code + PlatformIO Extension

```bash
1. Abre VS Code en la carpeta del proyecto
2. Presiona Ctrl+Shift+P
3. Escribe: "PlatformIO: Build"
4. Espera a que compile
```

Si todo va bien, deberías ver:
```
Compiling .pio/build/ttgo-t-watch/src/main.cpp.o
Compiling .pio/build/ttgo-t-watch/src/power_manager.cpp.o
Compiling .pio/build/ttgo-t-watch/src/bma_handler.cpp.o
Compiling .pio/build/ttgo-t-watch/src/fall_detector.cpp.o
Linking .pio/build/ttgo-t-watch/firmware.elf
...
✅ Build [ttgo-t-watch] succeeded
```

### Opción 2: Desde Terminal

```bash
cd "c:\Users\alext\Desktop\TFM\FallDetector_Watch"
pio run
```

---

## 🔌 Subir el Código al Reloj

### Opción 1: VS Code + PlatformIO

```bash
1. Conecta el reloj por USB
2. Presiona Ctrl+Shift+P
3. Escribe: "PlatformIO: Upload"
4. Espera a que se suba
```

### Opción 2: Terminal

```bash
pio run --target upload
```

---

## 📺 Ver Serial Monitor

### Opción 1: VS Code + PlatformIO

```bash
1. Ctrl+Shift+P
2. "PlatformIO: Serial Monitor"
3. Deberías ver logs del sistema
```

### Opción 2: Terminal

```bash
pio device monitor --baud 115200
```

---

## 🧪 Pruebas del Sistema

### ✓ Prueba 1: Verificar conexión

**Acción:**
1. Sube el código
2. Abre Serial Monitor
3. Observa los logs

**Esperado:**
```
╔════════════════════════════════════════╗
║   FALL DETECTOR - Sistema Iniciando    ║
╚════════════════════════════════════════╝

[BMAHandler] Inicializando BMA423...
[BMAHandler] ✓ BMA423 inicializado correctamente
[BMAHandler] ✓ Sensor online (Chip ID: 0x13)
[PowerManager] Configurando wake-up...
→ Volviendo a Deep Sleep...
[PowerManager] Entrando en Deep Sleep. Sensor vigilando...
```

**Si ves error:**
```
[BMAHandler] ✗ Error: Sensor OFFLINE (Chip ID: 0xXX)
```
→ Revisa la conexión I2C del sensor

---

### ✓ Prueba 2: Detectar Movimiento

**Acción:**
1. Sistema en Deep Sleep
2. **Agita suavemente el reloj**
3. Observa Serial Monitor

**Esperado:**
```
>>> MOVIMIENTO DETECTADO <<<
[FallDetector] Confirmando si es caída...
[FallDetector] Muestra 1: 3.45 m/s²
[FallDetector] Muestra 2: 2.12 m/s²
...
[FallDetector] Min: 1.23 | Max: 15.67 | Caída Libre: NO | Impacto: NO
✗ Falsa alarma - Movimiento normal
```

**Pantalla del reloj:**
```
┌─────────────────┐
│ MOVIMIENTO      │
│ Normal detectado│
└─────────────────┘
```

---

### ✓ Prueba 3: Detectar Caída (GOLPE FUERTE)

**Acción:**
1. Sistema en Deep Sleep
2. **Golpea fuerte la mesa CON el reloj**
3. Serial Monitor

**Esperado (Falsa Alarma):**
```
>>> MOVIMIENTO DETECTADO <<<
[FallDetector] Muestra 1: 35.45 m/s²   ← Solo impacto
[FallDetector] Muestra 2: 38.12 m/s²
...
[FallDetector] Min: 8.50 | Max: 42.33 | Caída Libre: NO | Impacto: SÍ
✗ Falsa alarma - Movimiento normal
```

---

### ✓ Prueba 4: Detectar Caída REAL (SOLTAR)

**Acción:**
1. Sistema en Deep Sleep
2. **Levanta el reloj 30cm y déjalo caer sobre la cama/colchón**
3. Serial Monitor + Pantalla

**Esperado (CAÍDA CONFIRMADA):**
```
>>> MOVIMIENTO DETECTADO <<<
[FallDetector] Muestra 1: 2.12 m/s²    ← Caída libre
[FallDetector] Muestra 2: 1.45 m/s²
[FallDetector] Muestra 3: 28.90 m/s²   ← Impacto
[FallDetector] Muestra 4: 35.67 m/s²
...
[FallDetector] Min: 0.45 | Max: 38.23 | Caída Libre: SÍ | Impacto: SÍ
[FallDetector] ⚠️  CAÍDA CONFIRMADA
✓ CAÍDA CONFIRMADA - Alerta enviada
```

**Pantalla del reloj:**
```
┌─────────────────┐
│ hola TFM        │
│ CAÍDA CONFIRMADA│
└─────────────────┘
```

**Motor:** Vibra 3 veces (confirmación)

---

## 🎯 Interpretando los Datos

### Ejemplo 1: Movimiento Normal
```
Min: 8.50 m/s² | Max: 15.67 m/s²
Caída Libre: NO | Impacto: NO
→ Resultado: IGNORAR
```

### Ejemplo 2: Golpe Normal (falsa alarma)
```
Min: 9.50 m/s² | Max: 42.33 m/s²
Caída Libre: NO | Impacto: SÍ
→ Resultado: IGNORAR (Golpe en mesa)
```

### Ejemplo 3: Caída Real ✓ POSITIVO
```
Min: 0.45 m/s² | Max: 38.23 m/s²
Caída Libre: SÍ | Impacto: SÍ
→ Resultado: ALERTA (CAÍDA CONFIRMADA)
```

---

## 🔧 Ajustar Sensibilidad

Si el sistema detecta demasiado/demasiado poco:

### Más sensible (detectar caídas más pequeñas)

**Archivo:** `fall_detector.cpp`

```cpp
// Bajar estos valores
const float FallDetector::FREE_FALL_THRESHOLD = 1.0;   // ← De 2.0 a 1.0
const float FallDetector::IMPACT_THRESHOLD = 15.0;     // ← De 25.0 a 15.0
```

### Menos sensible (ignorar movimientos bruscos pero no peligrosos)

```cpp
const float FallDetector::FREE_FALL_THRESHOLD = 3.0;   // ← De 2.0 a 3.0
const float FallDetector::IMPACT_THRESHOLD = 35.0;     // ← De 25.0 a 35.0
```

Luego:
```bash
pio run --target upload
```

---

## 📊 Consumo de Energía Esperado

| Estado | Corriente |
|--------|-----------|
| Deep Sleep (sensor activo) | ~50 µA |
| Despierto (procesando) | ~80-150 mA |
| Pantalla encendida | +200-300 mA |

**Autonomía estimada:**
- Con batería de 380 mAh: ~7-10 días (si se dispara 1 vez/día)

---

## 🆘 Troubleshooting

### ❌ "Sensor OFFLINE (Chip ID: 0xXX)"

**Posibles causas:**
- Conexión I2C suelta
- Sensor defectuoso
- Biblioteca desactualizada

**Solución:**
1. Revisa los cables SDA/SCL
2. Recarga el bootloader:
   ```bash
   pio run --target erase
   pio run --target upload
   ```

### ❌ No despierta con movimiento

**Posibles causas:**
- Interrupciones sin limpiar
- Pin 39 mal configurado
- Sensor no generando pulso

**Solución:**
1. Verifica en Serial:
   ```bash
   [PowerManager] Interrupciones limpias (5 intentos)
   ```
   Si dice > 100 intentos = registro atascado

2. Ajusta sensibilidad de BMA423 (si existe función en librería)

### ❌ El reloj se despierta indefinidamente

**Posible causa:**
- Pin 39 atascado en nivel ALTO

**Solución:**
1. Asegúrate que se limpian interrupciones:
   ```bash
   BMAHandler::clearAccelerometerInterrupts(ttgo);
   ```

2. Si el problema persiste, reinicia:
   ```bash
   pio device monitor --help  # Para ver opciones de reset
   ```

---

## 📈 Próximos Pasos

- [ ] Calibrar umbrales según el usuario
- [ ] Agregar estadísticas (# caídas detectadas)
- [ ] Conectar a WiFi/BLE
- [ ] Guardar datos en almacenamiento
- [ ] Interfaz gráfica mejorada

---

**¡Sistema listo para probar! 🚀**
