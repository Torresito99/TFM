#ifndef CONFIG_H
#define CONFIG_H

// --- Pin del pulsador ---
// Conectar un extremo del pulsador al GPIO y el otro a GND
// Se usa pull-up interno: en reposo HIGH, al pulsar LOW
#define BUTTON_PIN 14

// --- Debounce (ms) ---
#define DEBOUNCE_MS 50

#endif
