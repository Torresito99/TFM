# 📑 Índice de Documentación Completa

Bienvenido al proyecto **Fall Detector Watch**. Esta documentación te ayudará a entender, compilar, probar y mantener el sistema.

---

## 🚀 **COMIENZA AQUÍ** (Si tienes 5 minutos)

📄 **[INICIO_RAPIDO.md](INICIO_RAPIDO.md)**
- ⏱️ Guía de 5 minutos
- 🎯 Pasos esenciales: Compilar → Subir → Probar
- ✨ Checklist rápido
- 🔧 Ajustes básicos de sensibilidad

**Saltar aquí si:** Quieres que funcione ahora mismo

---

## 📚 **DOCUMENTACIÓN TÉCNICA COMPLETA**

### 1. 📋 **[RESUMEN.md](RESUMEN.md)** - Overview Total
- **Contenido:**
  - ✅ Lo que se ha hecho
  - 📁 Estructura del proyecto
  - 🎯 Funcionalidad implementada
  - ✨ Mejoras sobre el código anterior
  - 📊 Tabla comparativa

**Cuándo leer:** Entender qué cambió y por qué

---

### 2. 📖 **[SISTEMA.md](SISTEMA.md)** - Arquitectura del Sistema
- **Contenido:**
  - 🏗️ Estructura modular completa
  - 🔄 Flujo de funcionamiento (diagrama)
  - 📁 Descripción detallada de cada módulo
  - 🔌 Pines utilizados y configuración
  - 🎯 Caso de uso típico

**Cuándo leer:** Necesitas entender cómo todo funciona junto

---

### 3. 🎯 **[ALGORITMO.md](ALGORITMO.md)** - Lógica de Detección
- **Contenido:**
  - 🧮 Matemática detrás del algoritmo
  - 🎬 Fases de una caída real
  - 📊 Matriz de decisión
  - 🎚️ Parámetros ajustables (con explicaciones)
  - 📈 Procedimiento de calibración
  - 🔬 Física detrás del funcionamiento

**Cuándo leer:** 
- Necesitas ajustar sensibilidad
- Quieres entender la física
- Deseas mejorar el algoritmo

---

### 4. 🧪 **[TESTING.md](TESTING.md)** - Compilación y Pruebas
- **Contenido:**
  - 🔧 Cómo compilar (VS Code + Terminal)
  - 🔌 Cómo subir código al reloj
  - 📺 Configurar Serial Monitor
  - 🧪 4 casos de prueba completos
  - 📊 Interpretación de datos en Serial
  - 🎚️ Ajuste de sensibilidad paso a paso
  - 🆘 Troubleshooting detallado

**Cuándo leer:** 
- Primera vez compilando/subiendo
- Necesitas hacer pruebas
- Algo no funciona

---

### 5. 📊 **[ESTRUCTURA.md](ESTRUCTURA.md)** - Vista Completa del Proyecto
- **Contenido:**
  - 🗂️ Árbol completo del proyecto
  - 📌 Relación entre módulos (diagrama)
  - 🔍 Descripción de cada módulo
  - 🔄 Diagrama de flujo general
  - 🔄 Ciclo de vida del dispositivo
  - 🎯 Mapa de referencias rápidas

**Cuándo leer:** Necesitas visualizar todo el proyecto

---

### 6. 🔄 **[ANTES_DESPUES.md](ANTES_DESPUES.md)** - Transformación del Código
- **Contenido:**
  - 📊 Tabla comparativa antes/después
  - 🔴 Problemas del código original
  - 🟢 Soluciones implementadas
  - 📈 Comparación detallada de cada aspecto
  - 🎯 Resultados prácticos
  - 📊 Tabla completa de cambios
  - 💡 Lecciones aprendidas

**Cuándo leer:** 
- Quieres entender por qué cambiamos todo
- Necesitas evaluar mejoras
- Comparar código antes/después

---

## 🛠️ **ARCHIVOS DE CÓDIGO**

### Archivos NUEVOS (Creados)

**`src/power_manager.h` / `src/power_manager.cpp`**
- Gestión de energía y Deep Sleep
- Funciones: `init()`, `keepSensorPowered()`, `goToDeepSleep()`, `getWakeupReason()`

**`src/fall_detector.h` / `src/fall_detector.cpp`**
- Algoritmo inteligente de detección
- Funciones: `setupMotionDetection()`, `confirmFall()`, `debugPrintAccel()`
- Parámetros: `FREE_FALL_THRESHOLD`, `IMPACT_THRESHOLD`, `SAMPLE_COUNT`

### Archivos MODIFICADOS

**`src/main.cpp`** (Refactorizado)
- Antes: 95 líneas de spaghetti code
- Después: 140 líneas limpias y modular
- Incluye funciones helpers: `displayMessage()`, `vibrationFeedback()`, `handleMotionDetected()`

**`src/bma_handler.h` / `src/bma_handler.cpp`** (Mejorado)
- Refactorizado a namespace `BMAHandler::`
- Nueva función: `isSensorOnline()`
- Mejor logging con etiquetas

---

## 🗺️ **MAPA DE LECTURA POR CASO DE USO**

### Caso 1: "Acabo de descargar el proyecto, ¿por dónde empiezo?"
1. Lee: **INICIO_RAPIDO.md** (5 min)
2. Compila, sube, prueba
3. Cuando "hola TFM" funcione → Ya está

### Caso 2: "Quiero entender qué se hizo"
1. Lee: **RESUMEN.md** (10 min)
2. Lee: **ANTES_DESPUES.md** (10 min)
3. Opcional: **SISTEMA.md** para detalles

### Caso 3: "El reloj no detecta caídas / Hay demasiadas falsas alarmas"
1. Lee: **TESTING.md** → Troubleshooting
2. Lee: **ALGORITMO.md** → Calibración
3. Ajusta parámetros en `fall_detector.cpp`
4. Recompila y prueba

### Caso 4: "Necesito modificar el código / Mantener el proyecto"
1. Lee: **ESTRUCTURA.md** (entender módulos)
2. Lee: **SISTEMA.md** (de entender dependencias)
3. Lee fuente: código en `src/`
4. Serial logging te ayudará con debugging

### Caso 5: "Quiero entender la física detrás"
1. Lee: **ALGORITMO.md** (secciones matemática y física)
2. Experimenta: Deja caer el reloj, mira Serial
3. Entiende: `√(x²+y²+z²)` = magnitud de aceleración

### Caso 6: "¿Qué cambió del código original?"
1. Lee: **ANTES_DESPUES.md**
2. Compara: Viejo `main.cpp` vs nuevo `main.cpp`
3. Entiende: Modularidad vs spaghetti code

---

## 📊 **ESTADÍSTICAS DEL PROYECTO**

| Métrica | Valor |
|---------|-------|
| Archivos fuente | 7 |
| Líneas de código | ~400 |
| Archivos documentación | 7 |
| Líneas documentación | ~2000+ |
| Módulos | 3 (PowerManager, BMAHandler, FallDetector) |
| Funciones publicas | 12 |
| Parámetros ajustables | 3 |
| Casos de prueba | 4 |

---

## ✅ **CHECKLIST DE LECTURA**

Marca cada sección según tu necesidad:

- [ ] ⚡ INICIO_RAPIDO.md - Empezar rápido
- [ ] 📋 RESUMEN.md - Entender cambios
- [ ] 📖 SISTEMA.md - Arquitectura completa
- [ ] 🎯 ALGORITMO.md - Lógica de detección
- [ ] 🧪 TESTING.md - Compilación y pruebas
- [ ] 📊 ESTRUCTURA.md - Visualizar proyecto
- [ ] 🔄 ANTES_DESPUES.md - Ver transformación

---

## 🔗 **REFERENCIAS CRUZADAS**

### Si Necesitas → Ir a

| Necesito... | Archivo |
|------------|---------|
| Empezar ya mismo | INICIO_RAPIDO.md |
| Entender todo | RESUMEN.md + ESTRUCTURA.md |
| Cambiar sensibilidad | ALGORITMO.md § Calibración |
| Compilar/subir | TESTING.md § 1-2 |
| Probar | TESTING.md § 3-4 |
| Debuggear | TESTING.md § Troubleshooting |
| Ver parámetros | ALGORITMO.md § Parámetros |
| Entender módulos | ESTRUCTURA.md § Módulos |
| Ver cambios | ANTES_DESPUES.md |
| Física | ALGORITMO.md § Física |

---

## 🎯 **PUNTOS CLAVE DEL SISTEMA**

1. **Wake-on-Motion** → GPIO39 despierta ESP32
2. **Deep Sleep** → 50µA consumo (batería dura días)
3. **Algoritmo** → Detecta caída libre + impacto
4. **Modularidad** → 3 módulos separados
5. **Documentación** → 7 archivos técnicos

---

## 🚀 **PRÓXIMOS PASOS SUGERIDOS**

1. **Hoy:** Lee INICIO_RAPIDO.md y compila
2. **Mañana:** Lee TESTING.md y prueba todos casos
3. **Próxima semana:** Lee ALGORITMO.md y calibra
4. **Futuro:** Lee ANTES_DESPUES.md para entender decisiones

---

## 💬 **PREGUNTAS FRECUENTES**

**P: ¿Por dónde empiezo?**
A: Lee INICIO_RAPIDO.md (5 minutos)

**P: ¿Por qué 7 archivos de documentación?**
A: Porque el proyecto es profesional y escalable

**P: ¿Cuál es el algoritmo exacto?**
A: Ver ALGORITMO.md, sección "Matriz de Decisión"

**P: ¿Cómo cambio la sensibilidad?**
A: Ver ALGORITMO.md, sección "Calibración"

**P: ¿Cuánta batería consume?**
A: Ver SISTEMA.md, tabla consumo de energía

**P: ¿Por qué 3 módulos y no 1 archivo?**
A: Ver ANTES_DESPUES.md, sección "Modularidad"

---

## ✨ **CARACTERÍSTICAS DEL SISTEMA**

✅ Wake-on-Motion (GPIO39)
✅ Deep Sleep (50µA)
✅ Algoritmo inteligente de caídas
✅ 3 módulos reutilizables
✅ Parámetros ajustables
✅ Logging detallado
✅ Documentación profesional
✅ Casos de prueba completos

---

## 📞 **SOPORTE**

Si algo no funciona:
1. Abre **TESTING.md** → Troubleshooting
2. Revisa Serial Monitor
3. Ve logs etiquetados: `[PowerManager]`, `[BMAHandler]`, `[FallDetector]`

---

## 🎉 **¡BIENVENIDO AL PROYECTO!**

Ahora ya sabes dónde está todo. Elige por dónde empezar:

- ⚡ Rápido: **INICIO_RAPIDO.md**
- 📖 Detallado: **SISTEMA.md**
- 🎯 Técnico: **ALGORITMO.md**
- 🧪 Práctico: **TESTING.md**
- 🔄 Completo: **ANTES_DESPUES.md**

---

**Proyecto actualizado**: Marzo 2026
**Versión**: 1.0 - Fall Detector with Motion Detection
