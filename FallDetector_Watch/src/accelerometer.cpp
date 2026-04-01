/*******************************************************************************
 * @file    accelerometer.cpp
 * @brief   Configuración y lectura del acelerómetro BMA423
 ******************************************************************************/
#include "accelerometer.h"
#include "hardware.h"
#include <Arduino.h>
#include <cmath>

void accelInit()
{
    Serial.println("[ACCEL] Configurando BMA423...");

    bma->begin();
    bma->attachInterrupt();
    bma->enableAccel();

    // Configurar rango ±4g (caídas pueden superar 2g)
    Acfg cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_4G;
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
    cfg.perf_mode = BMA4_CIC_AVG_MODE;
    bma->accelConfig(cfg);

    // Activar feature de wakeup (any-motion) para despertar del deep sleep
    bma->enableFeature(BMA423_WAKEUP, true);

    // Configurar sensibilidad del wakeup: 0x07 = menos sensible (solo golpes fuertes)
    uint16_t rslt = bma423_wakeup_set_sensitivity(BMA423_WAKEUP_SENSITIVITY, bma->getDevice());
    if (rslt != BMA4_OK) {
        Serial.printf("[ACCEL] Error configurando sensibilidad wakeup: %u\n", rslt);
    } else {
        Serial.printf("[ACCEL] Sensibilidad wakeup: 0x%02X (0=max, 7=min)\n", BMA423_WAKEUP_SENSITIVITY);
    }

    // Mapear interrupción al pin INT
    bma->enableWakeupInterrupt(true);

    Serial.println("[ACCEL] BMA423 configurado — wakeup activo");
}

void accelConfigureWakeup()
{
    Serial.printf("[ACCEL] GPIO %d como fuente wake-up (nivel HIGH)\n", BMA423_INT_PIN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BMA423_INT_PIN, 1);
}

AccelSample accelReadSample()
{
    Accel raw;
    bma->getAccel(raw);

    // Debug: imprimir valores RAW una vez al inicio
    static bool rawPrinted = false;
    if (!rawPrinted) {
        Serial.printf("[ACCEL-RAW] raw.x=%d  raw.y=%d  raw.z=%d\n", raw.x, raw.y, raw.z);
        Serial.printf("[ACCEL-RAW] Conversión actual: * %.8f = x:%.4f y:%.4f z:%.4f g\n",
                      ACCEL_LSB_TO_G,
                      raw.x * ACCEL_LSB_TO_G,
                      raw.y * ACCEL_LSB_TO_G,
                      raw.z * ACCEL_LSB_TO_G);
        rawPrinted = true;
    }

    AccelSample s;
    s.x = raw.x * ACCEL_LSB_TO_G;
    s.y = raw.y * ACCEL_LSB_TO_G;
    s.z = raw.z * ACCEL_LSB_TO_G;
    s.magnitude = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);
    s.timestamp = millis();
    return s;
}

void accelCollectSamples(AccelBuffer &buf, uint32_t duration_ms)
{
    buf.reset();
    uint32_t start = millis();
    uint32_t nextSample = start;

    while ((millis() - start) < duration_ms) {
        if (millis() >= nextSample) {
            buf.push(accelReadSample());
            nextSample += SAMPLE_PERIOD_MS;
        }
        yield();  // Evitar watchdog reset
    }

    Serial.printf("[ACCEL] Recogidas %u muestras en %lu ms\n",
                  buf.count, millis() - start);
}
