/*******************************************************************************
 * @file    fall_detection.cpp
 * @brief   Detección de caídas mediante CNN 1D (TFLite Micro) + filtro físico
 *
 * @details Carga el modelo cuantizado INT8 desde flash y ejecuta inferencia
 *          sobre ventanas normalizadas del acelerómetro.
 *
 *          Mejoras V2:
 *            - Normalización por ventana (media=0, std=1) por eje
 *            - Filtro físico: exige fase de caída libre antes de confirmar
 *            - Esto elimina falsos positivos por agitar la mano
 ******************************************************************************/
#include "fall_detection.h"
#include "config.h"
#include "fall_model_data.h"
#include <Arduino.h>
#include <cmath>

#include <TensorFlowLite_ESP32.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

/* ─── Configuración del modelo ───────────────────────────────────────────── */
static constexpr int    TENSOR_ARENA_SIZE = 16 * 1024;  // 16KB arena
static constexpr int    MODEL_WINDOW_SIZE = 150;         // 3s @ 50Hz
static constexpr int    MODEL_NUM_FEATURES = 4;          // x, y, z, mag

/* ─── Sistema de puntuación multi-criterio ──────────────────────────────── */
// Cada evidencia física suma puntos. Umbral = 60 para confirmar caída.
static constexpr uint8_t SCORE_CNN_MAX       = 40;   // CNN contribuye hasta 40 pts
static constexpr uint8_t SCORE_FREEFALL_FULL = 20;   // Caída libre clara (mag < 0.5g, ≥3 muestras)
static constexpr uint8_t SCORE_FREEFALL_MILD = 10;   // Caída libre leve (mag < 0.7g)
static constexpr uint8_t SCORE_IMPACT_SEQ    = 25;   // Impacto > 2g DESPUÉS de caída libre
static constexpr uint8_t SCORE_IMPACT_ALONE  = 10;   // Impacto > 2g sin secuencia temporal
static constexpr uint8_t SCORE_ORIENTATION   = 15;   // Cambio de orientación > 40°
static constexpr uint8_t FALL_SCORE_MIN      = 60;   // Puntuación mínima para confirmar

/* ─── Umbrales físicos ──────────────────────────────────────────────────── */
static constexpr float  FREEFALL_G_STRONG = 0.5f;    // Caída libre clara
static constexpr float  FREEFALL_G_MILD   = 0.7f;    // Caída libre leve
static constexpr int    FREEFALL_MIN_COUNT = 3;       // Mínimo muestras consecutivas
static constexpr float  IMPACT_G          = 2.0f;     // Impacto mínimo (g)
static constexpr int    IMPACT_WINDOW     = 25;       // Muestras post-freefall para buscar impacto (500ms)
static constexpr float  ORIENT_ANGLE_DEG  = 40.0f;   // Cambio mínimo de orientación

/* ─── Estado del intérprete TFLite ───────────────────────────────────────── */
static uint8_t tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input_tensor = nullptr;
static TfLiteTensor *output_tensor = nullptr;
static bool model_loaded = false;

/* ─── Parámetros de cuantización ────────────────────────────────────────── */
static float input_scale = 1.0f;
static int32_t input_zero_point = 0;
static float output_scale = 1.0f;
static int32_t output_zero_point = 0;

bool fallDetectionInit()
{
    Serial.println("[NN] Inicializando TFLite Micro V2...");

    if (FALL_MODEL_SIZE == 0) {
        Serial.println("[NN] MODELO NO CARGADO — usa prepare_and_train.py");
        model_loaded = false;
        return false;
    }

    model = tflite::GetModel(fall_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[NN] Error: versión del modelo (%lu) != schema (%d)\n",
                      model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    static tflite::MicroErrorReporter micro_error_reporter;
    static tflite::AllOpsResolver resolver;

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, TENSOR_ARENA_SIZE, &micro_error_reporter);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[NN] Error al asignar tensores");
        return false;
    }

    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    if (input_tensor->type == kTfLiteInt8) {
        input_scale = input_tensor->params.scale;
        input_zero_point = input_tensor->params.zero_point;
        output_scale = output_tensor->params.scale;
        output_zero_point = output_tensor->params.zero_point;
        Serial.printf("[NN] Cuantización — Input: scale=%.6f zp=%d | Output: scale=%.6f zp=%d\n",
                      input_scale, input_zero_point, output_scale, output_zero_point);
    }

    Serial.printf("[NN] Modelo V2 cargado: %u bytes | Arena: %d bytes\n",
                  FALL_MODEL_SIZE, TENSOR_ARENA_SIZE);
    Serial.printf("[NN] Input: [%d, %d] | Output: [%d]\n",
                  input_tensor->dims->data[1], input_tensor->dims->data[2],
                  output_tensor->dims->data[1]);

    model_loaded = true;
    return true;
}

/*******************************************************************************
 * @brief   Busca fase de caída libre y devuelve el índice donde termina
 * @return  Índice de fin de freefall, o -1 si no se encontró
 ******************************************************************************/
static int findFreefallEnd(const AccelBuffer &buf, float threshold)
{
    int consecutive = 0;
    for (uint16_t i = 0; i < buf.count; i++) {
        if (buf.at(i).magnitude < threshold) {
            consecutive++;
            if (consecutive >= FREEFALL_MIN_COUNT) {
                return i;  // índice donde termina la caída libre
            }
        } else {
            consecutive = 0;
        }
    }
    return -1;
}

/*******************************************************************************
 * @brief   Busca impacto (> IMPACT_G) después de la caída libre
 *
 * La clave temporal: en una caída real, el impacto viene DESPUÉS de la
 * caída libre, no antes. Agitar la mano tiene picos sin freefall previo.
 ******************************************************************************/
static bool hasImpactAfterFreefall(const AccelBuffer &buf, int freefallEndIdx)
{
    if (freefallEndIdx < 0) return false;

    int searchEnd = min((int)buf.count, freefallEndIdx + IMPACT_WINDOW);
    for (int i = freefallEndIdx; i < searchEnd; i++) {
        if (buf.at(i).magnitude > IMPACT_G) {
            return true;
        }
    }
    return false;
}

/*******************************************************************************
 * @brief   Detecta cambio de orientación comparando inicio vs final del buffer
 *
 * En una caída, la persona pasa de vertical a horizontal. El vector de
 * gravedad promedio rota significativamente (> 40°).
 * Esto NO ocurre al agitar la mano o sentarse.
 ******************************************************************************/
static bool hasOrientationChange(const AccelBuffer &buf)
{
    if (buf.count < 60) return false;  // necesitamos al menos 60 muestras

    // Promediar primeras 30 muestras (0.6s de "antes")
    float ax1 = 0, ay1 = 0, az1 = 0;
    for (int i = 0; i < 30; i++) {
        ax1 += buf.at(i).x;
        ay1 += buf.at(i).y;
        az1 += buf.at(i).z;
    }
    ax1 /= 30.0f; ay1 /= 30.0f; az1 /= 30.0f;

    // Promediar últimas 30 muestras (0.6s de "después")
    float ax2 = 0, ay2 = 0, az2 = 0;
    int startLast = buf.count - 30;
    for (int i = startLast; i < (int)buf.count; i++) {
        ax2 += buf.at(i).x;
        ay2 += buf.at(i).y;
        az2 += buf.at(i).z;
    }
    ax2 /= 30.0f; ay2 /= 30.0f; az2 /= 30.0f;

    // Ángulo entre los dos vectores: cos(θ) = (a·b) / (|a|·|b|)
    float dot = ax1*ax2 + ay1*ay2 + az1*az2;
    float mag1 = sqrtf(ax1*ax1 + ay1*ay1 + az1*az1);
    float mag2 = sqrtf(ax2*ax2 + ay2*ay2 + az2*az2);

    if (mag1 < 0.1f || mag2 < 0.1f) return false;  // evitar división por ~0

    float cosAngle = dot / (mag1 * mag2);
    cosAngle = fmaxf(-1.0f, fminf(1.0f, cosAngle));  // clamp
    float angleDeg = acosf(cosAngle) * 180.0f / M_PI;

    Serial.printf("[PHYS] Orientación: %.1f° (umbral: %.0f°)\n", angleDeg, ORIENT_ANGLE_DEG);
    return angleDeg > ORIENT_ANGLE_DEG;
}

FallResult fallDetectionAnalyze(const AccelBuffer &buf)
{
    FallResult result = {};

    // ── Métricas básicas ──
    float peakG = 0.0f, minG = 100.0f;
    for (uint16_t i = 0; i < buf.count; i++) {
        float mag = buf.at(i).magnitude;
        if (mag > peakG) peakG = mag;
        if (mag < minG)  minG  = mag;
    }
    result.peakG = peakG;
    result.minG  = minG;

    // ── Análisis físico multi-criterio ──
    // 1. Caída libre (fuerte o leve)
    int ffEndStrong = findFreefallEnd(buf, FREEFALL_G_STRONG);
    int ffEndMild   = findFreefallEnd(buf, FREEFALL_G_MILD);
    result.freefallDetected = (ffEndMild >= 0);

    // 2. Impacto después de caída libre (secuencia temporal)
    int ffEnd = (ffEndStrong >= 0) ? ffEndStrong : ffEndMild;
    result.impactAfterFF = hasImpactAfterFreefall(buf, ffEnd);

    // 3. Cambio de orientación
    result.orientationChange = hasOrientationChange(buf);

    // ── Fallback sin modelo ──
    if (!model_loaded) {
        Serial.println("[NN] Modelo no cargado — usando solo física");
        result.probability = 0.0f;
        uint8_t s = 0;
        if (ffEndStrong >= 0) s += SCORE_FREEFALL_FULL;
        if (result.impactAfterFF) s += SCORE_IMPACT_SEQ;
        if (result.orientationChange) s += SCORE_ORIENTATION;
        result.score = s;
        result.detected = (s >= FALL_SCORE_MIN);
        return result;
    }

    // ── Preparar input para el modelo (valores en g directamente) ──
    uint32_t inferStart = millis();

    if (input_tensor->type == kTfLiteInt8) {
        int8_t *input_data = input_tensor->data.int8;

        for (int i = 0; i < MODEL_WINDOW_SIZE; i++) {
            float x, y, z, mag;
            if (i < (int)buf.count) {
                x   = buf.at(i).x;
                y   = buf.at(i).y;
                z   = buf.at(i).z;
                mag = buf.at(i).magnitude;
            } else {
                x = y = z = mag = 0.0f;
            }

            int idx = i * MODEL_NUM_FEATURES;
            input_data[idx + 0] = (int8_t)constrain((int)(x   / input_scale + input_zero_point), -128, 127);
            input_data[idx + 1] = (int8_t)constrain((int)(y   / input_scale + input_zero_point), -128, 127);
            input_data[idx + 2] = (int8_t)constrain((int)(z   / input_scale + input_zero_point), -128, 127);
            input_data[idx + 3] = (int8_t)constrain((int)(mag / input_scale + input_zero_point), -128, 127);
        }
    } else {
        float *input_data = input_tensor->data.f;

        for (int i = 0; i < MODEL_WINDOW_SIZE; i++) {
            int idx = i * MODEL_NUM_FEATURES;
            if (i < (int)buf.count) {
                input_data[idx + 0] = buf.at(i).x;
                input_data[idx + 1] = buf.at(i).y;
                input_data[idx + 2] = buf.at(i).z;
                input_data[idx + 3] = buf.at(i).magnitude;
            } else {
                input_data[idx + 0] = 0.0f;
                input_data[idx + 1] = 0.0f;
                input_data[idx + 2] = 0.0f;
                input_data[idx + 3] = 0.0f;
            }
        }
    }

    // ── Ejecutar inferencia ──
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("[NN] Error en inferencia");
        result.detected = false;
        result.probability = 0.0f;
        return result;
    }

    // ── Leer output ──
    if (output_tensor->type == kTfLiteInt8) {
        int8_t raw_output = output_tensor->data.int8[0];
        result.probability = (raw_output - output_zero_point) * output_scale;
    } else {
        result.probability = output_tensor->data.f[0];
    }

    result.probability = fmaxf(0.0f, fminf(1.0f, result.probability));
    result.inferenceTimeMs = millis() - inferStart;

    // ══════════════════════════════════════════════════════════════════════
    //  SISTEMA DE PUNTUACIÓN — cada evidencia suma puntos
    // ══════════════════════════════════════════════════════════════════════
    uint8_t score = 0;

    // CNN: contribuye hasta SCORE_CNN_MAX puntos proporcionalmente
    score += (uint8_t)(result.probability * SCORE_CNN_MAX);

    // Caída libre: fuerte (< 0.5g) = 20 pts, leve (< 0.7g) = 10 pts
    if (ffEndStrong >= 0) {
        score += SCORE_FREEFALL_FULL;
    } else if (ffEndMild >= 0) {
        score += SCORE_FREEFALL_MILD;
    }

    // Impacto DESPUÉS de caída libre = 25 pts (secuencia temporal correcta)
    // Impacto sin freefall previo = 10 pts (menos confiable)
    if (result.impactAfterFF) {
        score += SCORE_IMPACT_SEQ;
    } else if (peakG > IMPACT_G) {
        score += SCORE_IMPACT_ALONE;
    }

    // Cambio de orientación = 15 pts (persona tumbada)
    if (result.orientationChange) {
        score += SCORE_ORIENTATION;
    }

    result.score = score;
    result.detected = (score >= FALL_SCORE_MIN);

    Serial.printf("[SCORE] CNN=%u FF=%u IMP=%u ORI=%u → TOTAL=%u/%u → %s\n",
                  (uint8_t)(result.probability * SCORE_CNN_MAX),
                  (ffEndStrong >= 0) ? SCORE_FREEFALL_FULL : ((ffEndMild >= 0) ? SCORE_FREEFALL_MILD : 0),
                  result.impactAfterFF ? SCORE_IMPACT_SEQ : ((peakG > IMPACT_G) ? SCORE_IMPACT_ALONE : 0),
                  result.orientationChange ? SCORE_ORIENTATION : 0,
                  score, FALL_SCORE_MIN,
                  result.detected ? "CAIDA" : "OK");

    return result;
}

void fallDetectionPrintResult(const FallResult &result)
{
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║   DETECCIÓN DE CAÍDA — SCORING V3        ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  CNN:          %5.1f%% → %2u pts          \n",
                  result.probability * 100.0f, (uint8_t)(result.probability * SCORE_CNN_MAX));
    Serial.printf("║  Caída libre:  %s    → %2u pts          \n",
                  result.freefallDetected ? "SI" : "NO",
                  result.freefallDetected ? (result.minG < FREEFALL_G_STRONG ? SCORE_FREEFALL_FULL : SCORE_FREEFALL_MILD) : 0);
    Serial.printf("║  Impacto→FF:   %s    → %2u pts          \n",
                  result.impactAfterFF ? "SI" : "NO",
                  result.impactAfterFF ? SCORE_IMPACT_SEQ : ((result.peakG > IMPACT_G) ? SCORE_IMPACT_ALONE : 0));
    Serial.printf("║  Orientación:  %s    → %2u pts          \n",
                  result.orientationChange ? "SI" : "NO",
                  result.orientationChange ? SCORE_ORIENTATION : 0);
    Serial.printf("║  Peak: %.2fg  Min: %.2fg  %lums         \n",
                  result.peakG, result.minG, result.inferenceTimeMs);
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  SCORE: %3u / %u  →  %s       \n",
                  result.score, FALL_SCORE_MIN,
                  result.detected ? "!! CAIDA !!" : "OK");
    Serial.println("╚══════════════════════════════════════════╝\n");
}
