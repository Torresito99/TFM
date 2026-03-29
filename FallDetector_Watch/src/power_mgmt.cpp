/*******************************************************************************
 * @file    power_mgmt.cpp
 * @brief   Gestión de energía y deep sleep
 *
 * @details Apaga periféricos no esenciales y entra en deep sleep.
 *          Mantiene LDO3 (alimentación BMA423) para que la interrupción
 *          de wake-up funcione durante el sueño.
 ******************************************************************************/
#include "power_mgmt.h"
#include "hardware.h"
#include <Arduino.h>
#include <esp_sleep.h>

RTC_DATA_ATTR uint32_t wakeupCount = 0;

static void preparePeripherals()
{
    // Apagar pantalla
    watch->closeBL();
    watch->displaySleep();

    // Apagar salidas no esenciales del AXP202
    power->setPowerOutPut(AXP202_LDO2, false);  // Backlight
    power->setPowerOutPut(AXP202_LDO4, false);  // Audio

    // LDO3 se mantiene encendido — alimenta el BMA423 para interrupciones
    Serial.println("[PWR] Periféricos apagados para deep sleep");
}

void powerEnterDeepSleep()
{
    Serial.println("[PWR] ─────────────────────────────────");
    Serial.println("[PWR] Entrando en DEEP SLEEP...");
    Serial.println("[PWR] Wake-up por movimiento brusco");
    Serial.println("[PWR] ─────────────────────────────────");
    Serial.flush();

    preparePeripherals();
    delay(50);

    esp_deep_sleep_start();
    // Nunca se llega aquí
}

const char *powerGetWakeupReasonString()
{
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

    switch (reason) {
        case ESP_SLEEP_WAKEUP_EXT0:      return "BMA423 Motion";
        case ESP_SLEEP_WAKEUP_EXT1:      return "External INT";
        case ESP_SLEEP_WAKEUP_TIMER:     return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:  return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:       return "ULP";
        default:                         return "Power ON / Reset";
    }
}
