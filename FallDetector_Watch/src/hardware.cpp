/*******************************************************************************
 * @file    hardware.cpp
 * @brief   Inicialización del hardware del T-Watch V3
 ******************************************************************************/
#include "hardware.h"
#include "config.h"

TTGOClass    *watch = nullptr;
TFT_eSPI     *tft   = nullptr;
BMA           *bma   = nullptr;
AXP20X_Class *power = nullptr;

void hardwareInit()
{
    watch = TTGOClass::getWatch();
    watch->begin();

    tft   = watch->tft;
    power = watch->power;
    bma   = watch->bma;

    hardwareEnableDisplay();

    Serial.println("[HW] Hardware inicializado");
}

void hardwareInitMinimal()
{
    watch = TTGOClass::getWatch();
    watch->begin();

    tft   = watch->tft;
    power = watch->power;
    bma   = watch->bma;

    // No encender pantalla — solo necesitamos el acelerómetro
    Serial.println("[HW] Hardware mínimo inicializado");
}

void hardwareEnableDisplay()
{
    watch->openBL();
    watch->bl->adjust(BACKLIGHT_LEVEL);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
}
