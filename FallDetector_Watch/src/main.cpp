/*******************************************************************************
 * @file    main.cpp
 * @brief   FallDetector Watch — Detección de caídas CONTINUA
 *
 * @details Sistema de detección de caídas para LilyGO T-Watch V3 2020.
 *
 *          Flujo principal (modo continuo):
 *            1. Muestreo continuo del acelerómetro a 50Hz
 *            2. Buffer circular de 150 muestras (3 segundos)
 *            3. CNN ejecuta inferencia cada 1 segundo sobre el buffer
 *            4. Si detecta caída → alarma (vibración + sonido + pantalla)
 *            5. Pantalla muestra debug en tiempo real
 *
 * @hardware  LilyGO T-Watch V3 2020
 * @sensor    BMA423 (acelerómetro 3 ejes)
 * @author    Torres
 * @date      2025
 ******************************************************************************/

#include "config.h"
#include "hardware.h"
#include "accelerometer.h"
#include "fall_detection.h"
#include "alarm.h"
#include "display.h"
#include "power_mgmt.h"
#include <Arduino.h>

/* ─── Buffer de muestras (global) ────────────────────────────────────────── */
static AccelBuffer accelBuffer;

/* ─── Timers ─────────────────────────────────────────────────────────────── */
static uint32_t lastSampleTime = 0;
static uint32_t lastInferenceTime = 0;
static FallResult lastResult = {};

/* ─── Función de alarma ──────────────────────────────────────────────────── */
static void runAlarm(const FallResult &result)
{
    Serial.println("[MAIN] *** CAÍDA CONFIRMADA — ACTIVANDO ALARMA ***");
    displayFallDetected(result);
    delay(1000);

    alarmInit();

    uint32_t alarmStart = millis();
    bool vibState = false;
    uint32_t lastVibToggle = 0;
    bool toneHigh = true;
    uint32_t lastToneSwitch = 0;
    uint32_t lastDisplayUpdate = 0;

    while ((millis() - alarmStart) < ALARM_DURATION_MS) {
        uint32_t now = millis();

        uint32_t vibInterval = vibState ? VIBRATION_ON_MS : VIBRATION_OFF_MS;
        if ((now - lastVibToggle) >= vibInterval) {
            vibState = !vibState;
            if (vibState) {
                watch->motor->onec();
            }
            lastVibToggle = now;
        }

        if ((now - lastToneSwitch) >= TONE_SWITCH_MS) {
            uint16_t freq = toneHigh ? ALARM_TONE_HI_HZ : ALARM_TONE_LO_HZ;
            alarmPlayToneChunk(freq, TONE_SWITCH_MS);
            toneHigh = !toneHigh;
            lastToneSwitch = millis();
        }

        if ((now - lastDisplayUpdate) >= 500) {
            displayAlarm();
            lastDisplayUpdate = now;
        }

        yield();
    }

    alarmStop();
    Serial.println("[MAIN] Alarma finalizada");

    // Resetear buffer tras alarma para no re-detectar la misma caída
    accelBuffer.reset();
    lastResult = {};
}

/*******************************************************************************
 * @brief   Inicialización
 ******************************************************************************/
void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n═══════════════════════════════════════════");
    Serial.println("  FallDetector Watch — Modo Continuo");
    Serial.println("═══════════════════════════════════════════");

    // Inicializar hardware completo (con pantalla)
    hardwareInitMinimal();
    watch->motor_begin();  // Motor necesario para vibración en confirmación
    accelInit();
    hardwareEnableDisplay();
    fallDetectionInit();

    // Pantalla inicial
    tft->fillScreen(TFT_BLACK);
    tft->setTextSize(2);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("Llenando buffer...", 120, 100);
    tft->setTextSize(1);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("3 segundos", 120, 130);

    // Resetear buffer
    accelBuffer.reset();
    lastSampleTime = millis();
    lastInferenceTime = millis();
}

/*******************************************************************************
 * @brief   Bucle principal — muestreo + inferencia continua
 ******************************************************************************/
void loop()
{
    uint32_t now = millis();

    // ── Muestrear a 50Hz (cada 20ms) ──
    if ((now - lastSampleTime) >= SAMPLE_PERIOD_MS) {
        AccelSample s = accelReadSample();
        accelBuffer.push(s);
        lastSampleTime = now;

        // Imprimir valores RAW cada 500ms para diagnóstico
        static uint32_t lastRawPrint = 0;
        if ((now - lastRawPrint) >= 500) {
            Serial.printf("[RAW] x=%.3f y=%.3f z=%.3f mag=%.3f g\n",
                          s.x, s.y, s.z, s.magnitude);
            lastRawPrint = now;
        }
    }

    // ── Ejecutar CNN cada 1 segundo (si tenemos suficientes muestras) ──
    if ((now - lastInferenceTime) >= INFERENCE_INTERVAL_MS
        && accelBuffer.count >= MODEL_WINDOW_SAMPLES)
    {
        lastResult = fallDetectionAnalyze(accelBuffer);
        fallDetectionPrintResult(lastResult);

        displayDebug(lastResult);

        if (lastResult.detected) {
            // Fase de confirmación: 10 segundos para cancelar
            Serial.println("[MAIN] Caída detectada — esperando confirmación...");
            bool cancelled = displayConfirmation(10000);
            if (cancelled) {
                Serial.println("[MAIN] Alarma cancelada por el usuario");
                accelBuffer.reset();
                lastResult = {};
            } else {
                runAlarm(lastResult);
            }
        }

        lastInferenceTime = millis();
    }

    yield();
}
