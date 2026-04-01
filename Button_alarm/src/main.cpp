#include <Arduino.h>
#include "config.h"

uint32_t pressCount = 0;
bool lastState = HIGH;

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.println("=== Button Alarm - T-Energy S3 ===");
    Serial.printf("  Boton configurado en GPIO %d\n", BUTTON_PIN);
    Serial.println("  Esperando pulsacion...\n");
    Serial.flush();
}

void loop() {
    bool currentState = digitalRead(BUTTON_PIN);

    // Flanco de bajada: se acaba de pulsar
    if (lastState == HIGH && currentState == LOW) {
        pressCount++;
        Serial.printf(">>> BOTON PULSADO [%d] <<<\n", pressCount);
        Serial.flush();
        delay(DEBOUNCE_MS);
    }

    lastState = currentState;
    delay(10);
}
