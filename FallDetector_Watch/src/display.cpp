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

bool displayConfirmation(uint32_t timeout_ms)
{
    uint32_t start = millis();
    int16_t tx, ty;
    uint32_t lastSecond = 0xFFFF;  // forzar primer dibujo

    // Ignorar toques residuales: esperar a que se suelte la pantalla
    while (watch->getTouch(tx, ty)) {
        delay(50);
        if ((millis() - start) > 1000) break;  // máximo 1s esperando
    }

    // Vibración para avisar
    watch->motor->onec();

    // Dibujar partes fijas una sola vez
    tft->fillScreen(TFT_BLACK);

    // Cabecera roja
    tft->fillRect(0, 0, 240, 45, TFT_RED);
    tft->setTextSize(2);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawString("CAIDA DETECTADA", 120, 22);

    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("Alarma en segundos...", 120, 115);

    // Botón verde grande
    tft->fillRoundRect(30, 150, 180, 55, 10, TFT_GREEN);
    tft->setTextSize(2);
    tft->setTextColor(TFT_BLACK, TFT_GREEN);
    tft->drawString("ESTOY BIEN", 120, 177);

    tft->setTextSize(1);
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("Toca para cancelar", 120, 225);

    while ((millis() - start) < timeout_ms) {
        uint32_t elapsed = millis() - start;
        uint32_t remaining = (timeout_ms - elapsed) / 1000 + 1;

        // Solo redibujar el número cuando cambia el segundo
        if (remaining != lastSecond) {
            lastSecond = remaining;

            // Borrar zona del número y redibujar
            tft->fillRect(60, 55, 120, 50, TFT_BLACK);
            tft->setTextSize(4);
            tft->setTextColor(TFT_RED, TFT_BLACK);
            char buf[8];
            snprintf(buf, sizeof(buf), "%lu", remaining);
            tft->drawString(buf, 120, 80);

            // Vibración cada segundo
            watch->motor->onec();
        }

        // Comprobar touch — solo después de 500ms (evitar falsos por vibración)
        if (elapsed > 500 && watch->getTouch(tx, ty)) {
            // Cualquier toque en la pantalla cancela (el botón es grande)
            tft->fillScreen(TFT_BLACK);
            tft->setTextSize(2);
            tft->setTextColor(TFT_GREEN, TFT_BLACK);
            tft->drawString("Cancelado", 120, 120);
            delay(1500);
            return true;  // usuario canceló
        }

        delay(50);
    }

    return false;  // tiempo agotado → activar alarma
}

void displayDebug(const FallResult &result)
{
    tft->fillScreen(TFT_BLACK);
    char buf[48];

    // Cabecera — verde si OK, rojo si caída
    uint16_t headerColor = result.detected ? TFT_RED : 0x0400;
    tft->fillRect(0, 0, 240, 35, headerColor);
    tft->setTextSize(2);
    tft->setTextColor(TFT_WHITE, headerColor);
    tft->drawString(result.detected ? "CAIDA!" : "OK", 120, 17);

    // ── Score grande ──
    tft->setTextSize(3);
    uint16_t scoreColor = TFT_GREEN;
    if (result.score > 30) scoreColor = TFT_YELLOW;
    if (result.score >= 60) scoreColor = TFT_RED;
    tft->setTextColor(scoreColor, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%u/60", result.score);
    tft->drawString(buf, 120, 58);

    // ── 4 criterios con indicadores ──
    tft->setTextSize(1);
    int y = 88;

    // CNN
    uint8_t cnnPts = (uint8_t)(result.probability * 40);
    tft->setTextColor(cnnPts > 0 ? TFT_CYAN : TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "CNN %.0f%%          %2u pts", result.probability * 100.0f, cnnPts);
    tft->drawString(buf, 120, y); y += 18;

    // Freefall
    uint8_t ffPts = result.freefallDetected ? (result.minG < 0.5f ? 20 : 10) : 0;
    tft->setTextColor(ffPts > 0 ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Caida libre %s    %2u pts", result.freefallDetected ? "SI" : "NO", ffPts);
    tft->drawString(buf, 120, y); y += 18;

    // Impact after freefall
    uint8_t impPts = result.impactAfterFF ? 25 : (result.peakG > 2.0f ? 10 : 0);
    tft->setTextColor(impPts > 0 ? TFT_ORANGE : TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Impacto→FF  %s    %2u pts", result.impactAfterFF ? "SI" : "NO", impPts);
    tft->drawString(buf, 120, y); y += 18;

    // Orientation
    uint8_t oriPts = result.orientationChange ? 15 : 0;
    tft->setTextColor(oriPts > 0 ? TFT_MAGENTA : TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Orientacion %s    %2u pts", result.orientationChange ? "SI" : "NO", oriPts);
    tft->drawString(buf, 120, y); y += 18;

    // ── Separador ──
    tft->drawFastHLine(20, y, 200, TFT_DARKGREY); y += 8;

    // ── Datos aceleración ──
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Peak:%.1fg  Min:%.2fg  %lums", result.peakG, result.minG, result.inferenceTimeMs);
    tft->drawString(buf, 120, y); y += 16;

    // ── Barra de score visual ──
    tft->drawRect(20, y, 200, 14, TFT_WHITE);
    int barWidth = min(198, (int)(result.score * 198 / 100));
    if (barWidth > 0) {
        tft->fillRect(21, y + 1, barWidth, 12, scoreColor);
    }
    // Marca del umbral (60%)
    int threshX = 20 + (int)(60.0f / 100.0f * 200);
    tft->drawFastVLine(threshX, y - 2, 18, TFT_WHITE);
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
