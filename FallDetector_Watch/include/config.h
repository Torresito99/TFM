/*******************************************************************************
 * @file    config.h
 * @brief   Configuración central del sistema de detección de caídas
 *
 * @details Todos los parámetros ajustables del sistema están aquí.
 *          Modificar estos valores para calibrar la sensibilidad del
 *          algoritmo de detección de caídas.
 *
 * @hardware  LilyGO T-Watch V3 2020
 * @sensor    BMA423 (acelerómetro 3 ejes)
 ******************************************************************************/
#pragma once

#include <cstdint>

/* ─── Hardware Pins ───────────────────────────────────────────────────────── */
static constexpr uint8_t  BMA423_INT_PIN   = 39;
static constexpr uint8_t  BACKLIGHT_LEVEL  = 128;   // 0-255

/* ─── Acelerómetro ────────────────────────────────────────────────────────── */
static constexpr uint16_t ACCEL_SAMPLE_RATE_HZ = 50;    // 50Hz = igual que datos de entrenamiento
static constexpr uint16_t SAMPLE_PERIOD_MS     = 1000 / ACCEL_SAMPLE_RATE_HZ;  // 20ms
static constexpr uint16_t MODEL_WINDOW_SAMPLES = 150;   // 150 muestras = 3s @ 50Hz
static constexpr uint16_t MAX_SAMPLES          = MODEL_WINDOW_SAMPLES;
static constexpr uint16_t INFERENCE_INTERVAL_MS = 1000;  // Ejecutar CNN cada 1 segundo
static constexpr uint16_t ANALYSIS_WINDOW_MS   = 3000;  // Mantener por compatibilidad

// Factor de conversión LSB → g (BMA423 configurado a ±4g, 12-bit)
// ±4g con 12-bit: 1g = 512 LSB
static constexpr float    ACCEL_LSB_TO_G       = 1.0f / 512.0f;

/* ─── Umbrales de detección de caída (3 fases) ────────────────────────────── */

// Fase 1: Caída libre — aceleración cercana a 0g
// Una caída real produce ~0.1-0.3g durante 100-300ms (cuerpo en el aire)
// Movimientos normales (saludo, giro muñeca) rara vez bajan de 0.5g
static constexpr float    FREEFALL_THRESHOLD_G      = 0.4f;   // Magnitud < esto = caída libre
static constexpr uint8_t  FREEFALL_MIN_SAMPLES      = 10;     // ~50ms mínimo a 200Hz

// Fase 2: Impacto — pico alto de aceleración tras la caída libre
// Caída real: 6-15g al impactar.  Movimiento normal: 1-3g.
static constexpr float    IMPACT_THRESHOLD_G         = 4.0f;   // Magnitud > esto = impacto
static constexpr uint16_t IMPACT_WINDOW_SAMPLES      = 100;    // ~500ms ventana tras caída libre

// Fase 3: Post-impacto — cambio de orientación + inactividad
static constexpr float    ORIENTATION_CHANGE_DEG     = 50.0f;  // Grados de cambio = persona tumbada
static constexpr float    INACTIVITY_THRESHOLD_G     = 0.25f;  // Desviación estándar < esto = quieto
static constexpr uint16_t POST_IMPACT_WINDOW_SAMPLES = 200;    // ~1s de datos post-impacto

/* ─── Sistema de puntuación ───────────────────────────────────────────────── */
// IMPORTANTE: La caída libre es OBLIGATORIA para dar puntos de impacto completos.
// Sin caída libre previa, el impacto solo aporta puntos parciales (SCORE_IMPACT_NO_FF).
// Esto evita que movimientos bruscos normales (saludo, gesto) disparen la alarma.
static constexpr uint8_t  SCORE_FREEFALL     = 30;   // Puntos por detectar caída libre
static constexpr uint8_t  SCORE_IMPACT       = 30;   // Puntos por impacto CON caída libre previa
static constexpr uint8_t  SCORE_IMPACT_NO_FF = 10;   // Puntos por impacto SIN caída libre (penalizado)
static constexpr uint8_t  SCORE_ORIENTATION  = 25;   // Puntos por cambio de orientación
static constexpr uint8_t  SCORE_INACTIVITY   = 15;   // Puntos por inactividad post-impacto
static constexpr uint8_t  FALL_SCORE_THRESHOLD = 70;  // Score mínimo para confirmar caída

/* ─── Alarma ──────────────────────────────────────────────────────────────── */
static constexpr uint32_t ALARM_DURATION_MS   = 15000;  // Duración total de alarma (15s)
static constexpr uint16_t VIBRATION_ON_MS     = 500;    // Vibración ON
static constexpr uint16_t VIBRATION_OFF_MS    = 200;    // Vibración OFF
static constexpr uint16_t ALARM_TONE_HI_HZ   = 2000;   // Frecuencia alta sirena
static constexpr uint16_t ALARM_TONE_LO_HZ   = 1000;   // Frecuencia baja sirena
static constexpr uint16_t TONE_SWITCH_MS      = 500;    // Alternar tono cada X ms
static constexpr uint32_t I2S_SAMPLE_RATE     = 16000;  // Sample rate del speaker

/* ─── BMA423 Wake-up Sensitivity ─────────────────────────────────────────── */
// 0x00 = más sensible (cualquier movimiento), 0x07 = menos sensible (solo golpes fuertes)
static constexpr uint8_t  BMA423_WAKEUP_SENSITIVITY = 0x07;

/* ─── Deep Sleep ──────────────────────────────────────────────────────────── */
static constexpr uint32_t AWAKE_TIME_NO_FALL_MS = 500;  // Tiempo despierto si NO es caída (volver a dormir rápido)
