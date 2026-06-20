#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "config.h"
#include "alarms.h"
#include "ble_listener.h"   // escucha la baliza BLE de caida del reloj
#include "ha_notify.h"      // avisa a Home Assistant por MQTT (misma señal)

Adafruit_BME680 bme;
bool bmeOk = false;   // false si el sensor no esta conectado (el gateway sigue)

// Deteccion por flanco de la baliza de caida: solo avisamos UNA vez por
// episodio (cuando la baliza aparece tras estar ausente).
bool fallBeaconPrev = false;

// Referencia de gas en aire limpio (se calibra en los primeros minutos)
float gasBaseline = 0;
int baselineSamples = 0;
bool baselineReady = false;
#define BASELINE_SAMPLES 50

// Estado global de alarmas — accesible desde cualquier modulo
AlarmState alarmState;

// Estima IAQ (0-500) a partir de la resistencia del gas y humedad
float calcularIAQ(float gasRes, float hum) {
    if (!baselineReady) return -1;

    float gasScore = (gasRes / gasBaseline) * 100.0;
    if (gasScore > 100) gasScore = 100;

    float humScore = 100.0;
    if (hum < 38.0 || hum > 75.0) {
        humScore = 0;
    } else if (hum < 40.0) {
        humScore = (hum - 38.0) / 2.0 * 100.0;
    } else if (hum > 65.0) {
        humScore = (75.0 - hum) / 10.0 * 100.0;
    }

    float iaq = (gasScore * 0.75 + humScore * 0.25) * 5.0;
    return 500.0 - iaq;
}

float calcularECO2(float iaq) {
    return 400.0 + (iaq / 500.0) * 4600.0;
}

const char* etiquetaIAQ(float iaq) {
    if (iaq < 50)  return "Excelente";
    if (iaq < 100) return "Bueno";
    if (iaq < 150) return "Moderado";
    if (iaq < 200) return "Malo";
    if (iaq < 300) return "Muy malo";
    return "PELIGROSO";
}

// Agrega un evento al estado global de alarmas
void addAlarm(uint8_t code, AlarmLevel level, const char* id,
              const char* descripcion, const char* riesgo, const char* accion,
              float valor, float umbral) {
    if (alarmState.count >= MAX_ALARMAS) return;

    AlarmEvent& e = alarmState.eventos[alarmState.count];
    e.code = code;
    e.level = level;
    e.id = id;
    e.descripcion = descripcion;
    e.riesgo = riesgo;
    e.accion = accion;
    e.valor = valor;
    e.umbral = umbral;

    alarmState.activeFlags |= code;
    alarmState.count++;

    if (level == LEVEL_ALARM) alarmState.numAlarmas++;
    if (level == LEVEL_WARN)  alarmState.numAvisos++;
}

// Evalua todos los sensores y rellena alarmState
void evaluarAlarmas(float eco2, float gasRes, float temp, float hum) {
    // Resetear estado
    memset(&alarmState, 0, sizeof(alarmState));

    // CO2
    if (eco2 > CO2_THRESHOLD_PPM) {
        addAlarm(ALARM_CO2, LEVEL_ALARM, "CO2",
            "CO2 peligroso",
            "Confusion, mareos, agrava patologias cardiacas y pulmonares",
            "VENTILAR la zona inmediatamente",
            eco2, CO2_THRESHOLD_PPM);
    }

    // Gas nocivo
    if (gasRes < GAS_RESISTANCE_MIN_KOHM) {
        addAlarm(ALARM_GAS, LEVEL_ALARM, "GAS",
            "Gas nocivo detectado",
            "VOCs, humo o gases toxicos - menor percepcion olfativa en mayores",
            "EVACUAR y ventilar la zona",
            gasRes, GAS_RESISTANCE_MIN_KOHM);
    }

    // Temperatura alta
    if (temp > TEMP_MAX_C) {
        addAlarm(ALARM_TEMP_HIGH, LEVEL_ALARM, "TEMP_H",
            "Temperatura peligrosa (alta)",
            "Golpe de calor - mayores no regulan bien la temperatura",
            "Refrescar ambiente, hidratar, ropa ligera",
            temp, TEMP_MAX_C);
    } else if (temp > TEMP_WARN_C) {
        addAlarm(WARN_TEMP_HIGH, LEVEL_WARN, "TEMP_H_W",
            "Temperatura elevada",
            "Riesgo de deshidratacion y malestar",
            "Vigilar hidratacion, encender ventilador",
            temp, TEMP_WARN_C);
    }

    // Temperatura baja
    if (temp < TEMP_MIN_C) {
        addAlarm(ALARM_TEMP_LOW, LEVEL_ALARM, "TEMP_L",
            "Riesgo de hipotermia",
            "Mayores pierden calor corporal mas rapido",
            "Calentar ambiente, abrigar, bebida caliente",
            temp, TEMP_MIN_C);
    } else if (temp < TEMP_WARN_MIN_C) {
        addAlarm(WARN_TEMP_LOW, LEVEL_WARN, "TEMP_L_W",
            "Temperatura baja",
            "Puede causar malestar y rigidez muscular",
            "Encender calefaccion, abrigarse",
            temp, TEMP_WARN_MIN_C);
    }

    // Humedad alta
    if (hum > HUMIDITY_MAX) {
        addAlarm(ALARM_HUM_HIGH, LEVEL_ALARM, "HUM_H",
            "Humedad excesiva",
            "Dificulta respiracion, agrava EPOC y asma",
            "Deshumidificar, ventilar",
            hum, HUMIDITY_MAX);
    }

    // Humedad baja
    if (hum < HUMIDITY_MIN) {
        addAlarm(ALARM_HUM_LOW, LEVEL_ALARM, "HUM_L",
            "Aire demasiado seco",
            "Reseca mucosas, aumenta infecciones respiratorias",
            "Humidificar, hidratar",
            hum, HUMIDITY_MIN);
    }
}

// Imprime el estado de alarmas por Serial
void imprimirAlarmas() {
    if (alarmState.count == 0) {
        Serial.println("  Estado: Ambiente seguro para persona mayor");
        return;
    }

    for (uint8_t i = 0; i < alarmState.count; i++) {
        AlarmEvent& e = alarmState.eventos[i];
        if (e.level == LEVEL_ALARM) {
            Serial.printf("  !!! ALARMA [%s] - %s !!!\n", e.id, e.descripcion);
        } else {
            Serial.printf("  ** AVISO [%s] - %s **\n", e.id, e.descripcion);
        }
        Serial.printf("  Valor: %.1f | Umbral: %.1f\n", e.valor, e.umbral);
        Serial.printf("  >> Riesgo: %s\n", e.riesgo);
        Serial.printf("  >> Accion: %s\n", e.accion);
    }

    if (alarmState.numAlarmas > 0)
        Serial.printf("  >>> %d ALARMA(S) ACTIVA(S) <<<\n", alarmState.numAlarmas);
    if (alarmState.numAvisos > 0)
        Serial.printf("  >>> %d AVISO(S) PREVENTIVO(S) <<<\n", alarmState.numAvisos);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("=== Gateway Alarma: BME680 + baliza BLE caida -> Home Assistant ===");

    Wire.begin(I2C_SDA, I2C_SCL);

    bmeOk = bme.begin(0x77, &Wire) || bme.begin(0x76, &Wire);
    if (bmeOk) {
        Serial.println("[OK] Sensor BME680 detectado.");
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150);
        Serial.println("[OK] Sensor configurado. Iniciando lecturas...\n");
    } else {
        // No es fatal: la alarma de caida (BLE -> Home Assistant) es la
        // prioridad y debe funcionar aunque el BME680 no este conectado.
        Serial.println("[AVISO] No se encontro el BME680: se continua SIN medidas de ambiente.");
        Serial.printf("        Revisa el cableado: SDA -> GPIO%d, SCL -> GPIO%d\n", I2C_SDA, I2C_SCL);
    }

    // --- Gateway de caidas: WiFi/MQTT + escaneo BLE de la baliza ---
    haNotifyInit();      // WiFi + MQTT a Home Assistant (no bloqueante)
    bleListenerInit();   // empieza a escuchar la baliza "TFM-FALL"
}

// Relé de caida: si la baliza del reloj APARECE (flanco de subida),
// mandamos a Home Assistant la misma señal que enviaba el reloj.
void handleFallRelay() {
    bool present = bleFallBeaconPresent();

    if (present && !fallBeaconPrev) {
        Serial.println("\n############################################");
        Serial.println("###   BALIZA DE CAIDA DETECTADA (BLE)    ###");
        Serial.println("###   Avisando a Home Assistant...       ###");
        Serial.println("############################################\n");
        haNotifyFall();
    } else if (!present && fallBeaconPrev) {
        Serial.println("[BLE] La baliza de caida ha desaparecido (alarma finalizada)\n");
    }

    fallBeaconPrev = present;
}

// Lee el BME680, evalua alarmas de ambiente y lo muestra por el monitor.
void leerYMostrarBME680() {
    if (!bme.performReading()) {
        Serial.println("[ERROR] Fallo en la lectura del sensor.");
        return;
    }

    float temperatura = bme.temperature;
    float humedad = bme.humidity;
    float presion = bme.pressure / 100.0;
    float gasResistancia = bme.gas_resistance / 1000.0;

    // Calibracion de baseline
    if (!baselineReady) {
        gasBaseline += gasResistancia;
        baselineSamples++;
        Serial.printf("[CALIBRANDO] Muestra %d/%d - Gas: %.1f kOhm\n",
                       baselineSamples, BASELINE_SAMPLES, gasResistancia);
        if (baselineSamples >= BASELINE_SAMPLES) {
            gasBaseline /= baselineSamples;
            baselineReady = true;
            Serial.printf("[OK] Calibracion completada. Baseline: %.1f kOhm\n\n", gasBaseline);
        }
        return;
    }

    float iaq = calcularIAQ(gasResistancia, humedad);
    float eco2 = calcularECO2(iaq);

    // Evaluar alarmas de ambiente — rellena alarmState global
    evaluarAlarmas(eco2, gasResistancia, temperatura, humedad);

    // Mostrar lecturas por el monitor serie
    Serial.println("========================================");
    Serial.println("       LECTURA DEL SENSOR BME680       ");
    Serial.println("========================================");
    Serial.printf("  Temperatura:  %.1f C\n", temperatura);
    Serial.printf("  Humedad:      %.1f %%\n", humedad);
    Serial.printf("  Presion:      %.1f hPa\n", presion);
    Serial.printf("  Gas resist.:  %.1f kOhm\n", gasResistancia);
    Serial.println("----------------------------------------");
    Serial.printf("  IAQ:          %.0f (%s)\n", iaq, etiquetaIAQ(iaq));
    Serial.printf("  eCO2:         %.0f ppm\n", eco2);
    Serial.println("----------------------------------------");
    Serial.printf("  WiFi: %s | MQTT: %s\n",
                  haWifiConnected() ? "OK" : "--",
                  haMqttConnected() ? "OK" : "--");
    Serial.println("========================================");

    imprimirAlarmas();   // alarmas de ambiente: SOLO por pantalla (de momento)
    Serial.println();
}

void loop() {
    // 1) WiFi/MQTT y envio de avisos pendientes (no bloquea)
    haNotifyLoop();

    // 2) Relé de caida: reacciona al instante a la baliza BLE
    handleFallRelay();

    // 3) Lectura del BME680 cada READ_INTERVAL_MS (sin delay() bloqueante)
    static uint32_t lastRead = 0;
    uint32_t now = millis();
    if (bmeOk && now - lastRead >= READ_INTERVAL_MS) {
        lastRead = now;
        leerYMostrarBME680();
    }

    delay(5);  // pequeña cesion de CPU; el loop sigue siendo muy reactivo
}
