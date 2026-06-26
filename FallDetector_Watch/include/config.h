// =====================================================================
//  config.h — Pines y parámetros del Detector de Caídas
//  Placa: Waveshare ESP32-C6 2.06" Touch AMOLED
//  Pines tomados del pin_config.h y el BSP oficiales de Waveshare.
// =====================================================================
#pragma once

// ---------------------------------------------------------------------
//  DISPLAY  (CO5300, QSPI)
// ---------------------------------------------------------------------
#define LCD_SDIO0   1
#define LCD_SDIO1   2
#define LCD_SDIO2   3
#define LCD_SDIO3   4
#define LCD_SCLK    0
#define LCD_CS      5
#define LCD_RESET   11
#define LCD_WIDTH   410
#define LCD_HEIGHT  502
#define LCD_COL_OFFSET 22     // offset del panel (del ejemplo oficial)

// ---------------------------------------------------------------------
//  BUS I2C  (compartido: IMU + códec + touch + PMU + RTC)
// ---------------------------------------------------------------------
#define I2C_SDA     8
#define I2C_SCL     7

// ---------------------------------------------------------------------
//  TOUCH  (FT3168, I2C)
// ---------------------------------------------------------------------
#define TP_INT      15
#define TP_RST      10
#define TOUCH_ADDR  0x38     // dirección I2C del controlador táctil

// ---------------------------------------------------------------------
//  AUDIO  (códec ES8311 por I2S + amplificador)
// ---------------------------------------------------------------------
#define I2S_MCLK    19
#define I2S_BCLK    20
#define I2S_WS      22       // LRCK / word select
#define I2S_DOUT    23       // datos hacia el códec (altavoz)
#define I2S_DIN     21       // datos desde el códec (micro, no usado)
#define PA_ENABLE   6        // enable del amplificador de potencia
#define ES8311_ADDR 0x18     // dirección I2C del códec
#define AUDIO_SAMPLE_RATE 16000

// ---------------------------------------------------------------------
//  BOTÓN BOOT  (pin de strapping del ESP32-C6; pulsado = LOW)
// ---------------------------------------------------------------------
#define BOOT_BUTTON 9

// ---------------------------------------------------------------------
//  ALARMA
// ---------------------------------------------------------------------
#define PRE_ALARM_MS       10000   // cuenta atrás antes de que suene la alarma (cancelable)
#define MSG_DURATION_MS    2500    // tiempo que se muestra "cancelada"
// NOTA: una vez sonando, la alarma SOLO se para con el botón BOOT (no con la pantalla).

// ---------------------------------------------------------------------
//  RELOJ y BATERÍA  (intervalos de refresco, para ahorrar)
// ---------------------------------------------------------------------
#define BATTERY_REFRESH_MS  30000  // refresco del % de batería en pantalla
#define BLE_DEVICE_NAME     "TFM-FALL"   // nombre/baliza BLE al detectar caída

// ---------------------------------------------------------------------
//  AHORRO DE ENERGÍA  (Plan C: FIFO del IMU + light sleep)
// ---------------------------------------------------------------------
#define CPU_FREQ_MHZ        80      // baja la CPU de 160 a 80 MHz
#define SAMPLE_PERIOD_MS    10      // periodo de muestreo (~100 Hz) = duración de la micro-siesta
#define ENABLE_LIGHT_SLEEP  1       // 1=ahorra (batería, modo medida) · 0=depurar por USB
                                    //   OJO: con 1 el light sleep corta el USB-CDC nativo.
                                    //   Por eso NO dormimos si está enchufado (gOnUsb) y
                                    //   tampoco durante la ventana de arranque de abajo.
#define BOOT_NOSLEEP_MS     20000   // ventana sin dormir tras CADA arranque: garantiza que
                                    //   siempre se puede reflashear/recuperar por USB (red de
                                    //   seguridad para que el reloj nunca quede inaccesible).
#define DISPLAY_TIMEOUT_MS  20000   // apaga la pantalla tras este tiempo en reposo (0=siempre ON)
#define DISPLAY_BRIGHTNESS  160     // brillo del AMOLED (0-255)

// ---------------------------------------------------------------------
//  PRUEBA DE DURACIÓN DE BATERÍA  (TEMPORAL — poner a 0 para quitarlo)
//  Registra batería/voltaje en la flash (NVS) cada cierto tiempo, para
//  poder medir cuánto dura SIN estar conectado al PC. Al reconectar se
//  vuelca por serie escribiendo 'd' en el monitor. Gasto: despreciable.
// ---------------------------------------------------------------------
#define ENABLE_BATTLOG      1
#define BATTLOG_INTERVAL_MS 120000  // una muestra cada 2 min
#define BATTLOG_MAX         720     // hasta 720 muestras (~24 h)

// =====================================================================
//  ALGORITMO DE DETECCIÓN DE CAÍDAS  (reloj en la muñeca, personas
//  mayores). Una caída = secuencia OBLIGATORIA de 3 fases:
//    1) movimiento previo  ->  2) impacto brusco  ->  3) inmovilidad
//  Aceleración en g (1 g = gravedad). Giro en º/s (dps).
//  aDyn = |aMag - 1| = aceleración "dinámica" (sin la gravedad).
// =====================================================================

// --- Fase 1: movimiento previo al golpe (el brazo cae / se mueve) ---
#define PRE_WINDOW_MS       700     // ventana hacia atrás que se analiza
#define PRE_GYRO_MIN        60.0f   // giro mínimo previo (dps)  ...
#define PRE_ACT_MIN         0.30f   // ...o aceleración dinámica previa (g)

// --- Fase 2: impacto (pico brusco) ---
//  Debe cumplirse magnitud alta Y (jerk alto O giro alto).
#define IMPACT_G            2.30f   // pico de aceleración (más bajo: caídas blandas)
#define IMPACT_HARD_G       3.50f   // impacto fuerte (suma confianza)
#define JERK_THRESH         40.0f   // jerk mínimo del impacto (g/s)
#define JERK_STRONG         90.0f   // jerk fuerte (suma confianza)
#define GYR_IMPACT          100.0f  // giro en el impacto (dps)
#define GYR_IMPACT_STRONG   250.0f  // giro fuerte (suma confianza)

// --- Fase 3: inmovilidad / movimiento muy ligero tras el golpe ---
#define SETTLE_MS           300     // ventana tras impacto para capturar picos
#define STILL_ACT           0.18f   // aceleración dinámica por debajo = quieto (g)
#define STILL_GYR           25.0f   // giro por debajo = quieto (dps)
#define STILL_MS            1500    // hay que seguir quieto este tiempo
#define POST_TIMEOUT_MS     5000    // si vuelve a moverse normal, se descarta

// --- Orientación (de pie -> tumbado), refuerza la confianza ---
#define ORIENT_CHANGE_DEG   30.0f
#define ORIENT_CHANGE_BIG   60.0f

// --- Puntuación de confianza (las 3 fases ya son obligatorias) ---
//  Puntos: impacto>=IMPACT_G(+1), >=HARD(+1), jerk fuerte(+1),
//  giro>=GYR_IMPACT(+1), >=STRONG(+1), orientación>=DEG(+1), >=BIG(+1).
#define SCORE_THRESHOLD     3       // mínimo para confirmar caída
