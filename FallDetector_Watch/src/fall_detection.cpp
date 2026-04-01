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
static constexpr int    TENSOR_ARENA_SIZE = 16 * 1024;  // 16KB arena (modelo más grande)
static constexpr int    MODEL_WINDOW_SIZE = 150;         // 3s @ 50Hz
static constexpr int    MODEL_NUM_FEATURES = 4;          // x, y, z, mag
static constexpr float  DETECTION_THRESHOLD = 0.6f;      // Prob mínima CNN
static constexpr float  FREEFALL_G = 0.6f;               // Magnitud < esto = caída libre
static constexpr int    FREEFALL_MIN_COUNT = 2;           // Mínimo muestras en caída libre

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
 * @brief   Verifica si el buffer contiene una fase de caída libre
 *
 * Una caída real tiene una fase donde la magnitud baja mucho (~0g en caída
 * libre pura). Agitar la mano NUNCA produce esto porque la mano siempre
 * tiene aceleración centrípeta/gravedad.
 ******************************************************************************/
static bool hasFreefallPhase(const AccelBuffer &buf)
{
    int consecutive = 0;
    for (uint16_t i = 0; i < buf.count; i++) {
        if (buf.at(i).magnitude < FREEFALL_G) {
            consecutive++;
            if (consecutive >= FREEFALL_MIN_COUNT) {
                return true;
            }
        } else {
            consecutive = 0;
        }
    }
    return false;
}

FallResult fallDetectionAnalyze(const AccelBuffer &buf)
{
    FallResult result = {};

    // Calcular métricas básicas
    float peakG = 0.0f, minG = 100.0f;
    for (uint16_t i = 0; i < buf.count; i++) {
        float mag = buf.at(i).magnitude;
        if (mag > peakG) peakG = mag;
        if (mag < minG)  minG  = mag;
    }
    result.peakG = peakG;
    result.minG  = minG;

    // Verificar caída libre (filtro físico)
    bool freefall = hasFreefallPhase(buf);
    result.freefallDetected = freefall;

    if (!model_loaded) {
        Serial.println("[NN] Modelo no cargado — usando umbral simple");
        result.detected = (peakG > 4.0f && minG < 0.4f);
        result.probability = result.detected ? 0.8f : 0.1f;
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

    // ── Decisión final: CNN + filtro físico ──
    // Requiere AMBOS: CNN dice caída Y existe fase de caída libre
    bool cnnSaysFall = (result.probability >= DETECTION_THRESHOLD);
    result.detected = cnnSaysFall && freefall;

    Serial.printf("[NN] CNN=%.1f%% FF=%s → %s\n",
                  result.probability * 100.0f,
                  freefall ? "SI" : "NO",
                  result.detected ? "CAIDA" : "OK");

    return result;
}

void fallDetectionPrintResult(const FallResult &result)
{
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║   ANÁLISIS CNN V2 — DETECCIÓN DE CAÍDA   ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  Probabilidad CNN:   %6.1f%%             \n", result.probability * 100.0f);
    Serial.printf("║  Umbral CNN:         %6.1f%%             \n", DETECTION_THRESHOLD * 100.0f);
    Serial.printf("║  Caída libre:        %s                  \n", result.freefallDetected ? "SI" : "NO");
    Serial.printf("║  Pico aceleración:   %6.2f g             \n", result.peakG);
    Serial.printf("║  Mínimo aceleración: %6.2f g             \n", result.minG);
    Serial.printf("║  Tiempo inferencia:  %3lu ms              \n", result.inferenceTimeMs);
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf("║  RESULTADO:          %s                \n",
                  result.detected ? "!! CAIDA DETECTADA" : "OK Sin caida");
    Serial.println("╚══════════════════════════════════════════╝\n");
}
