#ifndef CONFIG_H
#define CONFIG_H

// --- Pines I2C para ESP32 Mini (WROOM-32, chip USB CH9102F) ---
// Pines I2C por defecto del ESP32 clasico. Conecta el BME680:
//   SDA -> GPIO21,  SCL -> GPIO22,  VCC -> 3V3,  GND -> GND
#define I2C_SDA 21
#define I2C_SCL 22
// (Placa antigua LilyGo T-Energy S3 usaba SDA=18, SCL=17)

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

// =====================================================================
//  BLE — Baliza de caida emitida por el reloj (FallDetector New Version)
//  El reloj hace advertising (sin conexion) con nombre "TFM-FALL" y unos
//  datos de fabricante que contienen "FALL". Este dispositivo escanea
//  continuamente y, al ver la baliza, dispara el aviso a Home Assistant.
// =====================================================================
#define BLE_GATEWAY_NAME      "TFM-GATEWAY" // nombre BLE de este dispositivo
#define BLE_BEACON_NAME       "TFM-FALL"    // nombre que emite el reloj al caer
#define BLE_SCAN_SECONDS      5             // duracion de cada ciclo de escaneo (se reanuda solo)
#define BLE_BEACON_TIMEOUT_MS 4000          // sin ver la baliza este tiempo => caida finalizada

// =====================================================================
//  MQTT / Home Assistant — MISMA señal que enviaba el reloj antiguo.
//  (Las credenciales/broker estan en secrets.h)
// =====================================================================
#define MQTT_TOPIC_FALL    "falldetector/fall"
#define MQTT_PAYLOAD_FALL  "{\"event\":\"fall_detected\"}"
#define MQTT_FALL_RETAINED true

#endif
