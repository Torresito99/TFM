#ifndef CONFIG_H
#define CONFIG_H

// Pulsador con pull-up interno: en reposo HIGH, pulsado LOW.
// Una patilla al GPIO, la otra a GND.
#define BUTTON_PIN 14

// Antirrebote del pulsador.
#define DEBOUNCE_MS 50

// Baliza BLE compartida con el reloj de caidas. El gateway C02 reacciona a este
// nombre, de modo que el boton dispara el mismo aviso que una caida.
#define BLE_DEVICE_NAME "TFM-FALL"

#endif
