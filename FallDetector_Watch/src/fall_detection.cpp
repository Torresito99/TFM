/*******************************************************************************
 * @file    fall_detection.cpp
 * @brief   Algoritmo de detección de caídas — 3 fases + puntuación ponderada
 *
 * @details El análisis se realiza sobre un buffer de muestras recogidas
 *          inmediatamente después de que el BMA423 despierte al ESP32.
 *
 *          Fases del algoritmo:
 *            1. Buscar caída libre (magnitud < FREEFALL_THRESHOLD_G)
 *            2. Buscar impacto (magnitud > IMPACT_THRESHOLD_G) tras caída libre
 *            3. Evaluar post-impacto: orientación + inactividad
 *
 *          Cada fase aporta puntos. Score >= FALL_SCORE_THRESHOLD = caída.
 ******************************************************************************/
#include "fall_detection.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>

/* ─── Helpers internos ────────────────────────────────────────────────────── */

/// Calcula el ángulo entre dos vectores de gravedad (grados)
static float angleBetweenVectors(float ax, float ay, float az,
                                  float bx, float by, float bz)
{
    float dot = ax * bx + ay * by + az * bz;
    float magA = sqrtf(ax * ax + ay * ay + az * az);
    float magB = sqrtf(bx * bx + by * by + bz * bz);

    if (magA < 0.01f || magB < 0.01f) return 0.0f;

    float cosAngle = dot / (magA * magB);
    // Clamp para evitar NaN por errores de precisión
    cosAngle = fmaxf(-1.0f, fminf(1.0f, cosAngle));
    return acosf(cosAngle) * 180.0f / M_PI;
}

/// Calcula la desviación estándar de magnitudes en un rango de muestras
static float stdDevMagnitude(const AccelBuffer &buf, uint16_t from, uint16_t count)
{
    if (count < 2) return 0.0f;

    float sum = 0.0f;
    float sumSq = 0.0f;

    for (uint16_t i = 0; i < count && (from + i) < buf.count; i++) {
        float m = buf.at(from + i).magnitude;
        sum += m;
        sumSq += m * m;
    }

    uint16_t n = (from + count <= buf.count) ? count : (buf.count - from);
    if (n < 2) return 0.0f;

    float mean = sum / n;
    float variance = (sumSq / n) - (mean * mean);
    return (variance > 0.0f) ? sqrtf(variance) : 0.0f;
}

/* ─── Análisis principal ──────────────────────────────────────────────────── */

FallResult fallDetectionAnalyze(const AccelBuffer &buf)
{
    FallResult result = {};

    if (buf.count < 20) {
        Serial.println("[FALL] Insuficientes muestras para análisis");
        return result;
    }

    // ── Variables de rastreo ──
    float peakG = 0.0f;
    float minG  = 100.0f;
    int16_t freefallStart = -1;
    int16_t freefallEnd   = -1;
    int16_t impactIndex   = -1;
    uint8_t freefallConsecutive = 0;

    // ── Fase 1 & 2: Buscar caída libre e impacto en una pasada ──
    for (uint16_t i = 0; i < buf.count; i++) {
        float mag = buf.at(i).magnitude;

        if (mag > peakG) peakG = mag;
        if (mag < minG)  minG  = mag;

        // Fase 1: Detectar caída libre
        if (mag < FREEFALL_THRESHOLD_G) {
            freefallConsecutive++;
            if (freefallConsecutive >= FREEFALL_MIN_SAMPLES && freefallStart < 0) {
                freefallStart = i - freefallConsecutive + 1;
            }
        } else {
            if (freefallConsecutive >= FREEFALL_MIN_SAMPLES && freefallEnd < 0) {
                freefallEnd = i;
            }
            freefallConsecutive = 0;
        }

        // Fase 2: Detectar impacto — solo marcar índice si supera umbral
        if (mag > IMPACT_THRESHOLD_G && impactIndex < 0) {
            impactIndex = i;
        }
    }

    // Almacenar métricas
    result.peakG = peakG;
    result.minG  = minG;

    // ── Verificar secuencia caída libre → impacto ──
    bool freefallDetected = (freefallStart >= 0 && minG < FREEFALL_THRESHOLD_G);
    bool impactDetected   = (peakG > IMPACT_THRESHOLD_G);
    bool impactAfterFreefall = false;

    if (freefallDetected && impactDetected && freefallEnd >= 0 && impactIndex >= 0) {
        impactAfterFreefall = ((impactIndex - freefallEnd) >= 0 &&
                               (impactIndex - freefallEnd) <= (int16_t)IMPACT_WINDOW_SAMPLES);
    }

    // ── Puntuación Fase 1: Caída libre ──
    if (freefallDetected) {
        result.scoreFreefall = SCORE_FREEFALL;
    }

    // ── Puntuación Fase 2: Impacto ──
    // Puntuación COMPLETA solo si hay caída libre previa confirmada.
    // Sin caída libre → puntuación reducida (evita falsos positivos por gestos)
    if (impactDetected) {
        result.scoreImpact = impactAfterFreefall ? SCORE_IMPACT : SCORE_IMPACT_NO_FF;
    }

    // ── Fase 3: Post-impacto — orientación + inactividad ──

    // Orientación: comparar vector de gravedad al inicio vs. al final del buffer
    // Al inicio el usuario estaba de pie/normal; al final tras el impacto
    uint16_t preWindow  = (buf.count > 40) ? 20 : buf.count / 4;
    uint16_t postStart  = (buf.count > 40) ? buf.count - 20 : buf.count * 3 / 4;
    uint16_t postWindow = buf.count - postStart;

    // Promedio de las primeras muestras (orientación pre-caída)
    float preX = 0, preY = 0, preZ = 0;
    for (uint16_t i = 0; i < preWindow; i++) {
        preX += buf.at(i).x;
        preY += buf.at(i).y;
        preZ += buf.at(i).z;
    }
    preX /= preWindow;  preY /= preWindow;  preZ /= preWindow;

    // Promedio de las últimas muestras (orientación post-impacto)
    float postX = 0, postY = 0, postZ = 0;
    for (uint16_t i = postStart; i < buf.count; i++) {
        postX += buf.at(i).x;
        postY += buf.at(i).y;
        postZ += buf.at(i).z;
    }
    postX /= postWindow;  postY /= postWindow;  postZ /= postWindow;

    result.orientationChangeDeg = angleBetweenVectors(preX, preY, preZ,
                                                       postX, postY, postZ);

    if (result.orientationChangeDeg > ORIENTATION_CHANGE_DEG) {
        result.scoreOrientation = SCORE_ORIENTATION;
    }

    // Inactividad post-impacto: baja desviación estándar en las últimas muestras
    result.postImpactStdDev = stdDevMagnitude(buf, postStart, postWindow);

    if (result.postImpactStdDev < INACTIVITY_THRESHOLD_G) {
        result.scoreInactivity = SCORE_INACTIVITY;
    }

    // ── Puntuación total ──
    result.score = result.scoreImpact + result.scoreFreefall +
                   result.scoreOrientation + result.scoreInactivity;
    result.detected = (result.score >= FALL_SCORE_THRESHOLD);

    return result;
}

void fallDetectionPrintResult(const FallResult &result)
{
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║     ANÁLISIS DE DETECCIÓN DE CAÍDA       ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  Pico aceleración:    %6.2f g            \n", result.peakG);
    Serial.printf("║  Mínimo aceleración:  %6.2f g            \n", result.minG);
    Serial.printf("║  Cambio orientación:  %6.1f°             \n", result.orientationChangeDeg);
    Serial.printf("║  StdDev post-impacto: %6.3f g            \n", result.postImpactStdDev);
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  Score Caída libre:   %3u / %u           \n", result.scoreFreefall, SCORE_FREEFALL);
    Serial.printf("║  Score Impacto:       %3u / %u           \n", result.scoreImpact, SCORE_IMPACT);
    Serial.printf("║  Score Orientación:   %3u / %u           \n", result.scoreOrientation, SCORE_ORIENTATION);
    Serial.printf("║  Score Inactividad:   %3u / %u           \n", result.scoreInactivity, SCORE_INACTIVITY);
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  SCORE TOTAL:         %3u / 100          \n", result.score);
    Serial.printf("║  UMBRAL:              %3u               \n", FALL_SCORE_THRESHOLD);
    Serial.printf("║  RESULTADO:           %s                \n",
                  result.detected ? "⚠ CAÍDA DETECTADA" : "✓ Sin caída");
    Serial.println("╚══════════════════════════════════════════╝\n");
}
