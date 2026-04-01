/*******************************************************************************
 * @file    fall_detection.h
 * @brief   Detección de caídas mediante CNN 1D + filtro físico (TFLite Micro)
 *
 * @details V2: CNN normalizada + filtro de caída libre.
 *          El modelo recibe ventanas normalizadas (media=0, std=1 por eje).
 *          Además exige una fase de caída libre para confirmar.
 ******************************************************************************/
#pragma once

#include "accelerometer.h"
#include <cstdint>

/// Resultado del análisis de caída
struct FallResult {
    bool     detected;          // true si se confirma caída (CNN + filtro físico)
    float    probability;       // Probabilidad CNN (0.0 - 1.0)
    float    peakG;             // Pico máximo de aceleración (g)
    float    minG;              // Mínimo de aceleración (g)
    bool     freefallDetected;  // true si se detectó fase de caída libre
    uint32_t inferenceTimeMs;   // Tiempo de inferencia (ms)
};

/// Inicializa el intérprete TFLite (llamar una vez al arrancar)
bool fallDetectionInit();

/// Analiza el buffer con la red neuronal y devuelve el resultado
FallResult fallDetectionAnalyze(const AccelBuffer &buf);

/// Imprime el resultado del análisis por Serial (debug)
void fallDetectionPrintResult(const FallResult &result);
