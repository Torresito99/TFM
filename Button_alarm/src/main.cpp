// Boton de panico. Al pulsarlo emite por BLE (advertising, sin conexion) la
// misma baliza que el reloj de caidas ("TFM-FALL" + datos de fabricante
// {0xFF,0xFF,'F','A','L','L'}), que el gateway C02 traduce en un aviso a Home
// Assistant. Cada pulsacion lanza una rafaga corta: el gateway la detecta y, al
// desaparecer, se rearma para el siguiente aviso.
#include <Arduino.h>
#include "config.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

static bool bleInited   = false;
static bool alarmActive = false;
static bool lastState   = HIGH;

// Duracion de la rafaga por pulsacion. Ha de bastar para que el escaneo del
// gateway la capte; tras apagarla, el gateway tarda BLE_BEACON_TIMEOUT_MS (4 s)
// en rearmarse, asi que conviene espaciar las pulsaciones ~5 s.
#define BEACON_BURST_MS 2000
static uint32_t beaconUntil = 0;

// Medida de bateria (divisor /2 en la T-Energy S3); no interviene en el BLE.
#define VBAT_PIN 3

// Arranca la emision de la baliza (mismo paquete que ble_alert del reloj).
void beaconStart() {
    if (alarmActive) return;

    if (!bleInited) {
        BLEDevice::init(BLE_DEVICE_NAME);
        bleInited = true;
    }

    BLEAdvertising *adv = BLEDevice::getAdvertising();

    BLEAdvertisementData advData;
    advData.setFlags(0x06);                 // BLE general, sin BR/EDR
    advData.setName(BLE_DEVICE_NAME);

    // Datos de fabricante: 0xFFFF (ID de prueba) + "FALL".
    String mfg;
    mfg += (char)0xFF; mfg += (char)0xFF;
    mfg += 'F'; mfg += 'A'; mfg += 'L'; mfg += 'L';
    advData.setManufacturerData(mfg);

    adv->setAdvertisementData(advData);
    adv->setMinInterval(0x20);              // ~20 ms
    adv->setMaxInterval(0x40);              // ~40 ms
    adv->start();

    alarmActive = true;
    Serial.println(">>> ALARMA ACTIVADA: emitiendo baliza BLE 'TFM-FALL' <<<");
    Serial.flush();
}

// Detiene la emision y libera la radio BLE; el gateway pasara a rearmarse.
void beaconStop() {
    if (!alarmActive) return;
    BLEDevice::getAdvertising()->stop();
    BLEDevice::deinit(true);
    bleInited   = false;
    alarmActive = false;
    Serial.println("Alarma cancelada, BLE apagado.");
    Serial.flush();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.println("=== Button Alarm (baliza BLE TFM-FALL) ===");
    Serial.printf("  Boton en GPIO %d. Pulsa para ACTIVAR / CANCELAR la alarma.\n", BUTTON_PIN);
    Serial.println("  Esperando pulsacion...\n");
    Serial.flush();
}

void loop() {
    bool currentState = digitalRead(BUTTON_PIN);

    // Flanco de bajada: pulsacion -> lanza una rafaga de aviso.
    if (lastState == HIGH && currentState == LOW) {
        beaconStart();                          // sin efecto si ya esta emitiendo
        beaconUntil = millis() + BEACON_BURST_MS;
        delay(DEBOUNCE_MS);                      // antirrebote
    }

    lastState = currentState;

    // Fin de la rafaga: apaga la baliza para que el gateway se rearme.
    if (alarmActive && (int32_t)(millis() - beaconUntil) >= 0) {
        beaconStop();
    }

    // Lectura de bateria cada 2 s (con el estado del pulsador para depurar).
    static uint32_t tBat = 0;
    if (millis() - tBat > 2000) {
        tBat = millis();
        uint32_t mv = analogReadMilliVolts(VBAT_PIN) * 2;  // x2 por el divisor
        Serial.printf("Bateria: %lu mV  (%.2f V)  |  GPIO%d = %d\n",
                      mv, mv / 1000.0, BUTTON_PIN, digitalRead(BUTTON_PIN));
        Serial.flush();
    }

    delay(10);
}
