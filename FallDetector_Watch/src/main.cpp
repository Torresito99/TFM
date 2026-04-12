#include "config.h"
#include "hardware.h"
#include "accelerometer.h"
#include "fall_detection.h"
#include "alarm.h"
#include "display.h"
#include "mqtt_client.h"
#include <Arduino.h>
#include <esp32-hal-cpu.h>

static constexpr uint32_t CPU_FREQ_NORMAL_MHZ = 80;
static constexpr uint32_t CPU_FREQ_ALARM_MHZ  = 240;
static constexpr uint32_t BATTERY_REPORT_MS   = 5UL * 60UL * 1000UL;  // 5 min

static AccelBuffer accelBuffer;
static uint32_t lastSampleTime    = 0;
static uint32_t lastInferenceTime = 0;
static uint32_t lastBatteryReport = 0;
static FallResult lastResult = {};

static void screenOff()
{
    watch->closeBL();
    watch->displaySleep();
    power->setPowerOutPut(AXP202_LDO2, false);
    power->setPowerOutPut(AXP202_LDO4, false);
}

// Tras displayWakeup hay que volver a fijar el datum, si no los textos
// centrados se dibujan desde la esquina y la pantalla queda descuadrada
static void screenOn()
{
    power->setPowerOutPut(AXP202_LDO2, true);
    watch->displayWakeup();
    watch->openBL();
    watch->bl->adjust(BACKLIGHT_LEVEL);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillScreen(TFT_BLACK);
}

static void runAlarmUntilTouch(const FallResult &result)
{
    Serial.println("[MAIN] Alarma activada");
    setCpuFrequencyMhz(CPU_FREQ_ALARM_MHZ);

    displayFallDetected(result);
    delay(1000);

    alarmInit();

    bool vibState = false;
    uint32_t lastVibToggle = 0;
    bool toneHigh = true;
    uint32_t lastToneSwitch = 0;
    uint32_t lastDisplayUpdate = 0;
    int16_t tx, ty;

    uint32_t flushStart = millis();
    while (watch->getTouch(tx, ty) && (millis() - flushStart) < 1000) {
        delay(50);
    }

    while (true) {
        uint32_t now = millis();

        uint32_t vibInterval = vibState ? VIBRATION_ON_MS : VIBRATION_OFF_MS;
        if ((now - lastVibToggle) >= vibInterval) {
            vibState = !vibState;
            if (vibState) watch->motor->onec();
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

        if (watch->getTouch(tx, ty)) {
            Serial.println("[MAIN] Detenida por el usuario");
            break;
        }

        yield();
    }

    alarmStop();

    tft->fillScreen(TFT_BLACK);
    tft->setTextSize(2);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("Alarma detenida", 120, 110);
    delay(1500);

    setCpuFrequencyMhz(CPU_FREQ_NORMAL_MHZ);
}

static void reportBattery()
{
    uint8_t  pct      = power->getBattPercentage();
    uint16_t voltage  = power->getBattVoltage();
    bool     charging = power->isChargeing();

    Serial.printf("[BAT] %u%% %umV %s\n", pct, voltage, charging ? "(cargando)" : "");
    mqttPublishBattery(pct, voltage, charging);
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\nFallDetector Watch");

    hardwareInitMinimal();
    watch->motor_begin();
    accelInit();
    fallDetectionInit();

    screenOff();
    setCpuFrequencyMhz(CPU_FREQ_NORMAL_MHZ);

    accelBuffer.reset();
    lastSampleTime    = millis();
    lastInferenceTime = millis();
    lastBatteryReport = millis();
}

void loop()
{
    uint32_t now = millis();

    if ((now - lastSampleTime) >= SAMPLE_PERIOD_MS) {
        AccelSample s = accelReadSample();
        accelBuffer.push(s);
        lastSampleTime = now;
    }

    if ((now - lastInferenceTime) >= INFERENCE_INTERVAL_MS
        && accelBuffer.count >= MODEL_WINDOW_SAMPLES)
    {
        lastResult = fallDetectionAnalyze(accelBuffer);
        fallDetectionPrintResult(lastResult);
        lastInferenceTime = millis();

        if (lastResult.detected) {
            Serial.println("[MAIN] Caida detectada");
            screenOn();
            bool cancelled = displayConfirmation(10000);

            if (cancelled) {
                Serial.println("[MAIN] Cancelado");
            } else {
                // Aviso a Home Assistant antes de la alarma local
                setCpuFrequencyMhz(CPU_FREQ_ALARM_MHZ);
                mqttPublishFall();
                runAlarmUntilTouch(lastResult);
            }

            screenOff();
            accelBuffer.reset();
            lastResult = {};
            lastSampleTime    = millis();
            lastInferenceTime = millis();
        }
    }

    static bool firstReportDone = false;
    uint32_t reportInterval = firstReportDone ? BATTERY_REPORT_MS : 5000;
    if ((now - lastBatteryReport) >= reportInterval) {
        firstReportDone = true;
        setCpuFrequencyMhz(CPU_FREQ_ALARM_MHZ);
        reportBattery();
        setCpuFrequencyMhz(CPU_FREQ_NORMAL_MHZ);
        lastBatteryReport = millis();
    }

    yield();
}
