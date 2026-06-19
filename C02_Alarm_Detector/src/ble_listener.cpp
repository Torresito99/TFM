#include "ble_listener.h"
#include "config.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static BLEScan* pScan = nullptr;
static volatile uint32_t lastSeenMs = 0;
static volatile bool everSeen = false;

// Callback que se dispara por cada anuncio BLE recibido durante el escaneo.
class BeaconCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    bool match = false;

    // 1) Por nombre del anuncio: "TFM-FALL"
    if (dev.haveName() && dev.getName() == BLE_BEACON_NAME) {
      match = true;
    }

    // 2) Por datos de fabricante: el reloj mete {0xFF,0xFF,'F','A','L','L'}
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

// Al terminar cada ciclo de escaneo, limpiamos resultados y lo reanudamos.
// Asi el escaneo es CONTINUO y NO bloquea el loop principal.
static void scanCompleteCB(BLEScanResults results) {
  pScan->clearResults();
  pScan->start(BLE_SCAN_SECONDS, scanCompleteCB, false);
}

void bleListenerInit() {
  BLEDevice::init(BLE_GATEWAY_NAME);
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new BeaconCallbacks(), true /*wantDuplicates*/);
  pScan->setActiveScan(true);   // pide scan response (mas fiable para el nombre)
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(BLE_SCAN_SECONDS, scanCompleteCB, false);  // arranque no bloqueante
  Serial.println("[BLE] Escaneando baliza de caida (TFM-FALL)...");
}

bool bleFallBeaconPresent() {
  if (!everSeen) return false;
  return (millis() - lastSeenMs) < BLE_BEACON_TIMEOUT_MS;
}

uint32_t bleLastSeenMs() {
  return lastSeenMs;
}
