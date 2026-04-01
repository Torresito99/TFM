/*******************************************************************************
 * @file    display.cpp
 * @brief   Pantallas UI del sistema de detección de caídas
 ******************************************************************************/
#include "display.h"
#include "hardware.h"
#include "config.h"
#include <Arduino.h>

void displayFallDetected(const FallResult &result)
{
    tft->fillScreen(TFT_BLACK);

    // Cabecera roja
    tft->fillRect(0, 0, 240, 50, TFT_RED);
    tft->setTextSize(2);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawString("CAIDA DETECTADA", 120, 25);

    // Datos del análisis
    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);

    char buf[48];
    snprintf(buf, sizeof(buf), "Probabilidad: %.0f%%", result.probability * 100.0f);
    tft->drawString(buf, 120, 70);

    snprintf(buf, sizeof(buf), "Pico: %.1fg  Min: %.2fg", result.peakG, result.minG);
    tft->drawString(buf, 120, 95);

    snprintf(buf, sizeof(buf), "Inferencia: %lu ms", result.inferenceTimeMs);
    tft->drawString(buf, 120, 115);

    // Estado
    tft->setTextColor(TFT_RED, TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString("ALARMA", 120, 155);
    tft->drawString("ACTIVADA", 120, 180);

    tft->setTextSize(1);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("Vibracion + sonido activos", 120, 220);
}

void displayDebug(const FallResult &result)
{
    tft->fillScreen(TFT_BLACK);

    // Cabecera — verde si OK, rojo si caída
    uint16_t headerColor = result.detected ? TFT_RED : 0x0400;  // verde oscuro
    tft->fillRect(0, 0, 240, 40, headerColor);
    tft->setTextSize(2);
    tft->setTextColor(TFT_WHITE, headerColor);
    tft->drawString(result.detected ? "CAIDA!" : "OK - Sin caida", 120, 20);

    // Probabilidad CNN (grande)
    tft->setTextSize(1);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("Probabilidad CNN:", 120, 55);

    tft->setTextSize(3);
    uint16_t probColor = TFT_GREEN;
    if (result.probability > 0.3f) probColor = TFT_YELLOW;
    if (result.probability > 0.6f) probColor = TFT_RED;
    tft->setTextColor(probColor, TFT_BLACK);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", result.probability * 100.0f);
    tft->drawString(buf, 120, 85);

    // Umbral
    tft->setTextSize(1);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("Umbral: 60%", 120, 115);

    // Caída libre: indicador visual clave
    tft->setTextSize(1);
    uint16_t ffColor = result.freefallDetected ? TFT_GREEN : TFT_DARKGREY;
    tft->setTextColor(ffColor, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Caida libre: %s", result.freefallDetected ? "SI" : "NO");
    tft->drawString(buf, 120, 130);

    // Línea separadora
    tft->drawFastHLine(20, 143, 200, TFT_DARKGREY);

    // Datos de aceleración
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(1);

    snprintf(buf, sizeof(buf), "Pico:  %.2f g", result.peakG);
    tft->drawString(buf, 120, 155);

    snprintf(buf, sizeof(buf), "Min:   %.2f g", result.minG);
    tft->drawString(buf, 120, 172);

    snprintf(buf, sizeof(buf), "Inferencia: %lu ms", result.inferenceTimeMs);
    tft->drawString(buf, 120, 189);

    // Barra visual de probabilidad
    tft->drawRect(20, 205, 200, 16, TFT_WHITE);
    int barWidth = (int)(result.probability * 198);
    if (barWidth > 0) {
        tft->fillRect(21, 206, barWidth, 14, probColor);
    }
    // Marca del umbral (60%)
    int threshX = 20 + (int)(0.6f * 200);
    tft->drawFastVLine(threshX, 203, 20, TFT_WHITE);

    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("0%       60%       100%", 120, 230);
}

void displayAlarm()
{
    // Pantalla de alarma con parpadeo rojo/negro
    static bool toggle = false;
    toggle = !toggle;

    uint16_t bg = toggle ? TFT_RED : TFT_BLACK;
    uint16_t fg = toggle ? TFT_WHITE : TFT_RED;

    tft->fillScreen(bg);
    tft->setTextSize(3);
    tft->setTextColor(fg, bg);
    tft->drawString("SOS", 120, 80);

    tft->setTextSize(2);
    tft->drawString("CAIDA", 120, 130);
    tft->drawString("DETECTADA", 120, 160);

    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, bg);
    tft->drawString("Alarma activa", 120, 210);
}
