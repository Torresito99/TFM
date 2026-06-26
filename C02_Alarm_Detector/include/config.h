#ifndef CONFIG_H
#define CONFIG_H

// Parametros del gateway: bus del sensor, umbrales ambientales, BLE y MQTT.

// Bus I2C del BME680 (ESP32 Mini / WROOM-32).
// Conexion: SDA->GPIO21, SCL->GPIO22, VCC->3V3, GND->GND.
#define I2C_SDA 21
#define I2C_SCL 22

// Umbrales ambientales ajustados a personas mayores, mas sensibles a la
// temperatura, la humedad y los gases que la poblacion general.
#define CO2_THRESHOLD_PPM       800    // eCO2 estimado (ppm)
#define GAS_RESISTANCE_MIN_KOHM 70     // por debajo: posibles gases nocivos

#define TEMP_MAX_C              32.0   // riesgo de golpe de calor
#define TEMP_WARN_C             28.0   // aviso de calor
#define TEMP_MIN_C              16.0   // riesgo de hipotermia
#define TEMP_WARN_MIN_C         18.0   // aviso de frio

#define HUMIDITY_MAX            70.0   // humedad excesiva (moho, disnea)
#define HUMIDITY_MIN            30.0   // aire demasiado seco

// Periodo de lectura del BME680 y de salida por el monitor serie.
#define READ_INTERVAL_MS 2000

// Baliza BLE del reloj de caidas. El gateway escanea de forma continua y
// dispara el aviso al detectarla.
#define BLE_GATEWAY_NAME      "TFM-GATEWAY"  // nombre BLE de este dispositivo
#define BLE_BEACON_NAME       "TFM-FALL"     // nombre que emite el reloj
#define BLE_SCAN_SECONDS      5              // duracion de cada ciclo de escaneo
#define BLE_BEACON_TIMEOUT_MS 4000           // ausencia que marca el fin de la alarma

// Aviso de caida a Home Assistant. Broker y credenciales en secrets.h.
#define MQTT_TOPIC_FALL    "falldetector/fall"
#define MQTT_PAYLOAD_FALL  "{\"event\":\"fall_detected\"}"
#define MQTT_FALL_RETAINED true

// Telemetria ambiental: envio periodico en un topic independiente del aviso de
// caida, con un JSON que agrupa todas las magnitudes del BME680.
#define MQTT_TOPIC_ENV     "c02alarm/ambiente"
#define MQTT_ENV_RETAINED  true
#define ENV_PUBLISH_MS     10000

#endif
