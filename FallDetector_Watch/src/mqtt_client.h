#pragma once

#include <cstdint>

/// Inicializa WiFi (modem sleep) y MQTT. Llamar una vez en setup().
void mqttInit();

/// Procesar cola y reconexiones. Llamar en cada loop() — NO bloquea.
void mqttLoop();

/// Encola evento de caída (retorna inmediatamente)
void mqttPublishFall();

/// Encola reporte de batería (retorna inmediatamente)
void mqttPublishBattery(uint8_t percentage, uint16_t voltage_mv, bool charging);
