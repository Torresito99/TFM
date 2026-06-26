#include "ble_listener.h"
#include "config.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static BLEScan* pScan = nullptr;
static volatile uint32_t lastSeenMs = 0;
static volatile bool everSeen = false;

class BeaconCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    bool match = false;

    // Coincidencia por nombre del anuncio.
    if (dev.haveName() && dev.getName() == BLE_BEACON_NAME) {
      match = true;
    }

    // Respaldo: datos de fabricante {0xFF,0xFF,'F','A','L','L'}.
    if (!match && dev.haveManufacturerData()) {
      String md = dev.getManufacturerData();
      if (md.indexOf("FALL") >= 0) match = true;
    }

    if (match) {
      lastSeenMs = millis();
      everSeen = true;
    }
  }
};

// Reanuda el escaneo al cerrar cada ciclo: asi es continuo y no bloquea el loop.
static void scanCompleteCB(BLEScanResults results) {
  pScan->clearResults();
  pScan->start(BLE_SCAN_SECONDS, scanCompleteCB, false);
}

void bleListenerInit() {
  BLEDevice::init(BLE_GATEWAY_NAME);
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new BeaconCallbacks(), true);
  pScan->setActiveScan(true);   // scan response: deteccion mas fiable del nombre
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(BLE_SCAN_SECONDS, scanCompleteCB, false);
  Serial.println("[BLE] Escaneando baliza de caida (TFM-FALL)...");
}

bool bleFallBeaconPresent() {
  if (!everSeen) return false;
  return (millis() - lastSeenMs) < BLE_BEACON_TIMEOUT_MS;
}

uint32_t bleLastSeenMs() {
  return lastSeenMs;
}
