#pragma once

#include <cstdint>

void mqttPublishFall();
void mqttPublishBattery(uint8_t percentage, uint16_t voltage_mv, bool charging);
