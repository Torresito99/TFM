# ESP32-C6 2.06" AMOLED Touch Watch - Proyecto Base

Configuración lista para programar el reloj ESP32-C6 con pantalla AMOLED Touch desde VSCode.

## Requisitos

1. **VSCode** instalado
2. **Extensión PlatformIO IDE** (instálala en VSCode)
3. **Drivers USB** del ESP32-C6 (generalmente se instalan automáticamente)
4. **Cable USB-C** para conectar el reloj

## Estructura del Proyecto

```
.
├── platformio.ini          # Configuración del proyecto
├── src/
│   └── main.cpp            # Código principal
├── include/
│   ├── lv_conf.h           # Config de LVGL
│   └── User_Setup.h        # Config de TFT_eSPI
└── lib/                    # Librerías (se descargan automáticamente)
```

## Configuración Rápida

### 1. Primer arranque
- Abre VSCode
- Abre esta carpeta como proyecto
- PlatformIO debería detectar automáticamente `platformio.ini`
- Verás "PlatformIO: Home" en la barra inferior

### 2. Conectar el reloj
- Conecta el ESP32-C6 al PC con cable USB-C
- PlatformIO detectará automáticamente el puerto
- Si no, edita `platformio.ini` y cambia:
  ```
  upload_port = COM3      # Cambia según tu puerto
  monitor_port = COM3
  ```

### 3. Compilar y cargar
Opción 1 - Desde VSCode:
- Abre la paleta de comandos: `Ctrl+Shift+P`
- Escribe: `PlatformIO: Upload`
- Espera a que compile e cargue

Opción 2 - Con los botones de PlatformIO:
- En la barra inferior verás los botones de build y upload
- Haz clic en el de flecha (upload)

### 4. Ver la salida serial
- Paleta de comandos: `Ctrl+Shift+P`
- Escribe: `PlatformIO: Monitor`
- Verás los mensajes del ESP32-C6

## ¿Qué debería ver?

En la pantalla del reloj:
- Texto blanco que dice **"estoy listo"** centrado

En el monitor serial:
```
=== ESP32-C6 Touch AMOLED Iniciándose ===
LVGL inicializado
Setup completado!
```

## Especificaciones del Hardware

- **Pantalla**: 2.06" AMOLED QSPI (410×502px)
- **Controlador Display**: CO5300
- **Touch**: FT3168 (I2C)
- **Procesador**: ESP32-C6 (RISC-V 32-bit)
- **WiFi**: WiFi 6 / Bluetooth 5 / Zigbee

## Solución de Problemas

### El reloj no se detecta
- Verifica que el cable USB-C esté bien conectado
- Prueba otro puerto USB
- Reinicia VSCode

### Error de compilación
- Limpia el proyecto: Paleta de comandos → `PlatformIO: Clean`
- Vuelve a compilar

### No aparece texto en pantalla
- Verifica los pines en `User_Setup.h` coincidan con tu hardware
- Comprueba que el backlight esté encendido (pin 0)

## Próximos Pasos

- Modifica `src/main.cpp` para agregar más elementos a la pantalla
- Lee la documentación de LVGL: https://docs.lvgl.io/
- Experimenta con colores, fonts, etc.

## Librerías Instaladas

- **LVGL** v8.4.0 - GUI gráfica
- **TFT_eSPI** - Control del display SPI
- **Adafruit GFX** - Gráficos básicos
- **Wire** - I2C (para el touch)

---

¡Listo! Ahora puedes programar el reloj. 🚀
