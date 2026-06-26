#include "ble_alert.h"
#include "config.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

static bool _bleInited = false;
static bool _active    = false;

void bleAlertStart() {
  if (_active) return;

  if (!_bleInited) {
    BLEDevice::init(BLE_DEVICE_NAME);
    _bleInited = true;
  }

  BLEAdvertising *adv = BLEDevice::getAdvertising();

  // Paquete de anuncio: nombre + datos de fabricante reconocibles
  BLEAdvertisementData advData;
  advData.setFlags(0x06);                 // BLE general, sin BR/EDR
  advData.setName(BLE_DEVICE_NAME);

  // Datos de fabricante: ID de prueba 0xFFFF + "FALL"
  String mfg;
  mfg += (char)0xFF; mfg += (char)0xFF;
  mfg += 'F'; mfg += 'A'; mfg += 'L'; mfg += 'L';
  advData.setManufacturerData(mfg);

  adv->setAdvertisementData(advData);
  adv->setMinInterval(0x20);              // ~20 ms
  adv->setMaxInterval(0x40);              // ~40 ms
  adv->start();

  _active = true;
  Serial.println("[ble] EMITIENDO baliza de caida (TFM-FALL)");
}

void bleAlertStop() {
  if (!_active) return;
  BLEDevice::getAdvertising()->stop();
  BLEDevice::deinit(true);                // libera memoria y apaga la radio
  _bleInited = false;
  _active    = false;
  Serial.println("[ble] baliza detenida, BLE apagado");
}

bool bleAlertActive() {
  return _active;
}
