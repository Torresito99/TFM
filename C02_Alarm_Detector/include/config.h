#ifndef CONFIG_H
#define CONFIG_H

// --- Pines I2C para LilyGo T-Energy S3 ---
#define I2C_SDA 18
#define I2C_SCL 17

// --- Umbrales de alarma (adaptados a personas mayores) ---
// Personas mayores tienen menor capacidad de termorregulacion,
// mayor sensibilidad respiratoria y menor percepcion de riesgo.

#define CO2_THRESHOLD_PPM       800    // Mas bajo que 1000 general: mayores son mas sensibles
#define GAS_RESISTANCE_MIN_KOHM 70     // Umbral mas alto: detectar antes gases nocivos

// Temperatura: mayores sufren golpe de calor desde 35C y hipotermia bajo 16C
#define TEMP_MAX_C              32.0   // Riesgo de golpe de calor en mayores
#define TEMP_WARN_C             28.0   // Aviso preventivo de calor
#define TEMP_MIN_C              16.0   // Riesgo de hipotermia en mayores
#define TEMP_WARN_MIN_C         18.0   // Aviso preventivo de frio

// Humedad: vias respiratorias mas vulnerables en mayores
#define HUMIDITY_MAX            70.0   // Dificulta respiracion, favorece moho
#define HUMIDITY_MIN            30.0   // Reseca mucosas, aumenta infecciones

// --- Intervalo de lectura (ms) ---
#define READ_INTERVAL_MS 2000

#endif
