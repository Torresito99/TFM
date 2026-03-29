/*******************************************************************************
 * @file    display.cpp
 * @brief   Pantallas UI del sistema de detección de caídas
 ******************************************************************************/
#include "display.h"
#include "hardware.h"
#include "config.h"
#include <Arduino.h>

void displayAnalyzing()
{
    tft->fillScreen(TFT_BLACK);

    tft->setTextSize(2);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->drawString("ANALIZANDO", 120, 80);

    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("Movimiento detectado", 120, 120);
    tft->drawString("Recogiendo datos...", 120, 145);

    // Barra de progreso vacía
    tft->drawRect(30, 175, 180, 16, TFT_WHITE);
}

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
    snprintf(buf, sizeof(buf), "Score: %u / 100", result.score);
    tft->drawString(buf, 120, 70);

    snprintf(buf, sizeof(buf), "Pico: %.1fg  Min: %.2fg", result.peakG, result.minG);
    tft->drawString(buf, 120, 95);

    snprintf(buf, sizeof(buf), "Orientacion: %.0f grados", result.orientationChangeDeg);
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

void displayNoCaida(const FallResult &result)
{
    tft->fillScreen(TFT_BLACK);

    tft->setTextSize(2);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("SIN CAIDA", 120, 40);

    tft->drawFastHLine(20, 65, 200, TFT_GREEN);

    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);

    char buf[48];
    snprintf(buf, sizeof(buf), "Score: %u / %u", result.score, FALL_SCORE_THRESHOLD);
    tft->drawString(buf, 120, 90);

    snprintf(buf, sizeof(buf), "Pico: %.1fg  Min: %.2fg", result.peakG, result.minG);
    tft->drawString(buf, 120, 115);

    snprintf(buf, sizeof(buf), "Orientacion: %.0f grados", result.orientationChangeDeg);
    tft->drawString(buf, 120, 140);

    tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    tft->drawString("Volviendo a dormir...", 120, 180);

    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("Falso positivo descartado", 120, 220);
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
