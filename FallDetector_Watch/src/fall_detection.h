/*******************************************************************************
 * @file    fall_detection.h
 * @brief   Algoritmo de detección de caídas — 3 fases + puntuación ponderada
 *
 * @details Algoritmo basado en literatura científica de detección de caídas:
 *
 *   Fase 1 — CAÍDA LIBRE: Magnitud de aceleración cercana a 0g durante un
 *            período mínimo (indica cuerpo en caída libre).
 *
 *   Fase 2 — IMPACTO: Pico alto de aceleración (>3g) que ocurre tras la
 *            caída libre (indica impacto contra el suelo).
 *
 *   Fase 3 — POST-IMPACTO: Cambio de orientación respecto a la posición
 *            previa + inactividad prolongada (persona tumbada e inmóvil).
 *
 *   Cada fase contribuye una puntuación parcial. Si la suma supera el
 *   umbral FALL_SCORE_THRESHOLD se confirma la caída.
 ******************************************************************************/
#pragma once

#include "accelerometer.h"
#include <cstdint>

/// Resultado detallado del análisis de caída
struct FallResult {
    bool     detected;            // true si se confirma caída
    uint8_t  score;               // Puntuación total (0-100)

    // Métricas individuales
    float    peakG;               // Pico máximo de aceleración (g)
    float    minG;                // Mínimo de aceleración (caída libre) (g)
    float    orientationChangeDeg;// Cambio de orientación (grados)
    float    postImpactStdDev;    // Desviación estándar post-impacto (g)

    // Puntuaciones parciales
    uint8_t  scoreImpact;
    uint8_t  scoreFreefall;
    uint8_t  scoreOrientation;
    uint8_t  scoreInactivity;
};

/// Analiza el buffer de muestras y devuelve el resultado de detección
FallResult fallDetectionAnalyze(const AccelBuffer &buf);

/// Imprime el resultado del análisis por Serial (debug)
void fallDetectionPrintResult(const FallResult &result);
