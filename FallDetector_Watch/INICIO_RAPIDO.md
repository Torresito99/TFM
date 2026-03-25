# ⚡ INICIO RÁPIDO - Guía 5 Minutos

## 🎯 Objetivo
Detectar caídas con un reloj inteligente usando:
- Deep Sleep para ahorrar batería
- BMA423 como "perro guardián" (despierta el reloj)
- Algoritmo inteligente para confirmar caídas reales

---

## ⏱️ En 5 Pasos

### 1️⃣ Compilar el Código (2 min)

**En VS Code:**
- Abre la carpeta del proyecto
- Presiona: `Ctrl + Shift + P`
- Escribe: `PlatformIO: Build`
- Espera a que termine

**Esperado:**
```
✓ Compiling successful
✓ No errors
```

---

### 2️⃣ Conectar el Reloj (30 seg)

- Conecta el reloj por USB al PC
- Debería reconocerse automáticamente

---

### 3️⃣ Subir el Código (1 min)

**En VS Code:**
- Presiona: `Ctrl + Shift + P`
- Escribe: `PlatformIO: Upload`
- Espera a que cargue

**Esperado:**
```
✓ Uploading [████████████] 100%
✓ Build succeeded
```

---

### 4️⃣ Abrir Serial Monitor (30 seg)

**En VS Code:**
- Presiona: `Ctrl + Shift + P`
- Escribe: `PlatformIO: Serial Monitor`

**Esperado:**
```
╔════════════════════════════════════════╗
║   FALL DETECTOR - Sistema Iniciando    ║
╚════════════════════════════════════════╝
[BMAHandler] Sensor online (Chip ID: 0x13)
→ Volviendo a Deep Sleep...
```

---

### 5️⃣ Probar el Sistema (1 min)

**Acción 1: Agita el reloj**
```
Monitor Serial muestra:
>>> MOVIMIENTO DETECTADO <<<
[FallDetector] Confirmando si es caída...
✗ Falsa alarma - Movimiento normal
```
→ **Pantalla muestra:** "Movimiento / Normal detectado"

**Acción 2: Golpea fuerte la mesa CON el reloj**
```
Monitor Serial muestra:
Max: 35.0 m/s² | Min: 8.5 m/s²
✗ Falsa alarma (solo impacto)
```

**Acción 3: Suelta el reloj desde 30cm**
```
Monitor Serial muestra:
Max: 38.0 m/s² | Min: 0.5 m/s²
[FallDetector] ⚠️ CAÍDA CONFIRMADA
```
→ **Pantalla muestra:** "hola TFM" + **Vibra 3 veces** ✅

---

## 📋 Checklist Rápido

- [ ] Proyecto abre en VS Code
- [ ] `Ctrl+Shift+P` → Build → Sin errores
- [ ] Reloj conectado por USB
- [ ] `Ctrl+Shift+P` → Upload → Cargado
- [ ] Serial Monitor abierto
- [ ] Agitar → Despierta correctamente
- [ ] Soltar desde 30cm → Muestra "hola TFM"

---

## 🚨 Si Algo Falla

| Problema | Solución |
|----------|----------|
| "Build failed" | Check TESTING.md → Troubleshooting |
| Sensor offline | Revisa conexión I2C del BMA423 |
| No despierta | Agita más fuerte o lee ALGORITMO.md |
| Siempre dispara | Aumentar umbrales: ver abajo |

---

## 🔧 Ajuste Rápido de Sensibilidad

**Si NO detecta caídas (muy poco sensible):**

Edita `src/fall_detector.cpp` línea ~7:
```cpp
const float FallDetector::IMPACT_THRESHOLD = 20.0;  // ← Cambiar 25 por 20
```

Recompila y prueba.

**Si SIEMPRE DETECTA caídas (muy sensible):**

Edita `src/fall_detector.cpp` línea ~6:
```cpp
const float FallDetector::FREE_FALL_THRESHOLD = 3.0;  // ← Cambiar 2 por 3
```

---

## 📚 Documentación Disponible

- 📖 **SISTEMA.md** → Arquitectura completa del sistema
- 🧪 **TESTING.md** → Guía detallada de pruebas
- 🎯 **ALGORITMO.md** → Explicación técnica del algoritmo
- 📋 **RESUMEN.md** → Resumen de todos los cambios

---

## 🎓 ¿Cómo Funciona?

```
┌─────────────┐
│  PROFUNDO   │  Reloj durmiendo
│  SLEEP      │  50 µA consumo
└─────────────┘
        ↓ (MOVIMIENTO)
┌─────────────┐
│  DESPIERTA  │  BMA423 envía pulso
│  ESP32      │
└─────────────┘
        ↓
┌─────────────┐
│  LEE 10     │  Confirma caída
│  MUESTRAS   │  vs golpe normal
└─────────────┘
        ↓
   SÍ caída        NO falsa alarma
      ↓               ↓
  "hola TFM"      "Movimiento Normal"
   Vibra 3x       Vibra 1x
```

---

## 💡 Puntos Clave

✅ **El reloj NO está siempre despierto** (batería dura días)
✅ **El BMA423 actúa como alarma** (el "perro guardián")
✅ **El algoritmo es inteligente** (no dispara por cada golpe)
✅ **Código limpio y modular** (fácil de entender y modificar)
✅ **Serial logging para debugging** (sé qué está pasando)

---

## 🎯 Ejemplo de Uso Real

**La abuela lleva el reloj mientras camina:**

1. Reloj dur miendo (ahorra batería)
2. Abuela se cae al suelo
3. En < 200ms: "hola TFM" aparece en pantalla
4. Reloj vibra 3 veces
5. Familia puede ser avisada (futuro)

**Sin este sistema:**
- Reloj siempre despierto = batería dura 2 horas
- Pantalla siempre encendida = quema energía
- No hay detección automática = dependencia manual

---

## 🚀 ¿Listo?

1. Abre VS Code
2. Build: `Ctrl+Shift+P` → Build
3. Upload: `Ctrl+Shift+P` → Upload
4. Monitor: `Ctrl+Shift+P` → Serial Monitor
5. ¡Prueba agitando el reloj!

---

**¡Éxito! 🎉**

Si tienes dudas, consulta los archivos `.md` incluidos.
