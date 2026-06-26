#pragma once

// Escaneo de la baliza BLE de caida ("TFM-FALL"), que el reloj emite por
// advertising. Expone si la baliza esta presente para que el gateway decida.

#include <cstdint>

// Arranca el escaneo continuo. Llamar una vez en setup().
void bleListenerInit();

// true mientras la baliza se haya visto dentro de la ventana de timeout.
bool bleFallBeaconPresent();

// Instante (millis) de la ultima deteccion; 0 si nunca se ha visto.
uint32_t bleLastSeenMs();
