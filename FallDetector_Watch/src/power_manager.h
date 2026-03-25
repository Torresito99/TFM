#pragma once
#include <Arduino.h>
#include <LilyGoWatch.h>

#define BMA_INT_PIN 39

// Gestión de energía y modos de sueño
class PowerManager {
public:
    static void init(TTGOClass *ttgo);
    static void goToDeepSleep(TTGOClass *ttgo);
    static esp_sleep_wakeup_cause_t getWakeupReason();
    static void keepSensorPowered(TTGOClass *ttgo);
};
