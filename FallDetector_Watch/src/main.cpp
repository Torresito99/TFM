/*******************************************************************************
 * @file    main.cpp
 * @brief   FallDetector Watch — Sistema de detección de caídas
 *
 * @details Sistema de detección de caídas para LilyGO T-Watch V3 2020.
 *
 *          Flujo principal:
 *            1. Deep sleep continuo (consumo mínimo)
 *            2. BMA423 detecta movimiento brusco → despierta ESP32
 *            3. ESP32 recoge muestras del acelerómetro (~3 segundos)
 *            4. Algoritmo de 3 fases analiza si es una caída real:
 *               - Fase 1: Caída libre (aceleración ~0g)
 *               - Fase 2: Impacto (pico alto de aceleración)
 *               - Fase 3: Post-impacto (cambio orientación + inactividad)
 *            5a. Si es caída → alarma (vibración + sonido + pantalla)
 *            5b. Si no es caída → vuelve a deep sleep inmediatamente
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

/* ─── Buffer de muestras (global para evitar uso excesivo de stack) ─────── */
static AccelBuffer accelBuffer;

/*******************************************************************************
 * @brief   Flujo principal al despertar
 ******************************************************************************/
void setup()
{
    Serial.begin(115200);
    delay(100);

    wakeupCount++;

    Serial.println("\n═══════════════════════════════════════════");
    Serial.println("  FallDetector Watch — T-Watch V3");
    Serial.printf("  Wake-up #%u | Razón: %s\n", wakeupCount, powerGetWakeupReasonString());
    Serial.println("═══════════════════════════════════════════");

    // ── Paso 1: Inicializar hardware mínimo (sin pantalla aún) ──
    hardwareInitMinimal();
    accelInit();
    accelConfigureWakeup();

    // ── Paso 2: Recoger muestras del acelerómetro (SIN pantalla — ahorro de batería) ──
    Serial.printf("[MAIN] Recogiendo datos durante %u ms...\n", ANALYSIS_WINDOW_MS);
    accelCollectSamples(accelBuffer, ANALYSIS_WINDOW_MS);

    // ── Paso 3: Analizar muestras con el algoritmo de detección ──
    Serial.println("[MAIN] Ejecutando algoritmo de detección de caídas...");
    FallResult result = fallDetectionAnalyze(accelBuffer);
    fallDetectionPrintResult(result);

    // ── Paso 4: Actuar según resultado ──
    if (result.detected) {
        // ═══ CAÍDA DETECTADA ═══
        Serial.println("[MAIN] *** CAÍDA CONFIRMADA — ACTIVANDO ALARMA ***");

        // Ahora SÍ encender pantalla — solo cuando hay caída real
        hardwareEnableDisplay();
        displayFallDetected(result);
        delay(1000);  // Mostrar info antes de la alarma visual

        // Iniciar alarma (vibración + sonido)
        alarmInit();

        // Bucle de alarma con pantalla parpadeante + sonido + vibración
        uint32_t alarmStart = millis();
        bool vibState = false;
        uint32_t lastVibToggle = 0;
        bool toneHigh = true;
        uint32_t lastToneSwitch = 0;
        uint32_t lastDisplayUpdate = 0;

        while ((millis() - alarmStart) < ALARM_DURATION_MS) {
            uint32_t now = millis();

            // Vibración on/off
            uint32_t vibInterval = vibState ? VIBRATION_ON_MS : VIBRATION_OFF_MS;
            if ((now - lastVibToggle) >= vibInterval) {
                vibState = !vibState;
                if (vibState) {
                    watch->motor->onec();
                }
                lastVibToggle = now;
            }

            // Sirena: alternar tono alto/bajo por el speaker
            if ((now - lastToneSwitch) >= TONE_SWITCH_MS) {
                uint16_t freq = toneHigh ? ALARM_TONE_HI_HZ : ALARM_TONE_LO_HZ;
                alarmPlayToneChunk(freq, TONE_SWITCH_MS);
                toneHigh = !toneHigh;
                lastToneSwitch = millis();
            }

            // Actualizar pantalla (parpadeo cada 500ms)
            if ((now - lastDisplayUpdate) >= 500) {
                displayAlarm();
                lastDisplayUpdate = now;
            }

            yield();
        }

        alarmStop();
        Serial.println("[MAIN] Alarma finalizada");
    } else {
        // ═══ FALSO POSITIVO — No es caída ═══
        Serial.println("[MAIN] No es caída — volviendo a dormir silenciosamente");
    }

    // ── Paso 5: Volver a deep sleep ──
    powerEnterDeepSleep();
}

/*******************************************************************************
 * @brief   Loop — no se ejecuta en este diseño
 ******************************************************************************/
void loop()
{
    // El flujo es: deep sleep → wake → setup() → deep sleep
    // Este loop nunca se alcanza
}
