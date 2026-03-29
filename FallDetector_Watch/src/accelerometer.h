/*******************************************************************************
 * @file    accelerometer.h
 * @brief   Interfaz del acelerómetro BMA423
 ******************************************************************************/
#pragma once

#include <cstdint>
#include "config.h"

/// Muestra de aceleración con marca temporal
struct AccelSample {
    float x;            // Aceleración eje X (g)
    float y;            // Aceleración eje Y (g)
    float z;            // Aceleración eje Z (g)
    float magnitude;    // Magnitud = sqrt(x² + y² + z²)
    uint32_t timestamp; // millis() en el momento de la lectura
};

/// Buffer circular de muestras
struct AccelBuffer {
    AccelSample samples[MAX_SAMPLES];
    uint16_t count;
    uint16_t head;     // Índice de escritura

    void reset() { count = 0; head = 0; }

    void push(const AccelSample &s)
    {
        samples[head] = s;
        head = (head + 1) % MAX_SAMPLES;
        if (count < MAX_SAMPLES) count++;
    }

    /// Acceso por índice relativo (0 = más antiguo)
    const AccelSample &at(uint16_t i) const
    {
        uint16_t idx = (count < MAX_SAMPLES)
                       ? i
                       : (head + i) % MAX_SAMPLES;
        return samples[idx];
    }
};

/// Configura el BMA423 para detección de caídas (rango, ODR, interrupciones)
void accelInit();

/// Configura la interrupción de wake-up en el ESP32 (ext0 en GPIO 39)
void accelConfigureWakeup();

/// Lee una muestra del acelerómetro y la devuelve convertida a g
AccelSample accelReadSample();

/// Recoge muestras durante `duration_ms` y las almacena en el buffer
void accelCollectSamples(AccelBuffer &buf, uint32_t duration_ms);
