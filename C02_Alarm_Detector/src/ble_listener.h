// =====================================================================
//  ble_listener.h — Escucha la baliza BLE de caida del reloj.
//
//  El reloj (FallDetector New Version) EMITE (advertising, sin conexion)
//  un paquete con nombre "TFM-FALL" cuando suena la alarma de caida.
//  Aqui escaneamos continuamente y exponemos si la baliza esta presente.
// =====================================================================
#pragma once

#include <cstdint>

// Inicia el escaneo BLE continuo. Llamar una vez en setup().
void bleListenerInit();

// true mientras la baliza de caida se este viendo (refrescado por escaneo).
bool bleFallBeaconPresent();

// millis() de la ultima vez que se vio la baliza (0 si nunca).
uint32_t bleLastSeenMs();
