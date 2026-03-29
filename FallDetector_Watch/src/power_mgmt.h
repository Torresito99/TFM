/*******************************************************************************
 * @file    power_mgmt.h
 * @brief   Gestión de energía y deep sleep del ESP32
 ******************************************************************************/
#pragma once

#include <cstdint>

/// Contador de wake-ups (persistente en RTC memory, sobrevive al deep sleep)
extern uint32_t wakeupCount;

/// Prepara periféricos y entra en deep sleep (no retorna)
void powerEnterDeepSleep();

/// Devuelve la razón del último wake-up como string
const char *powerGetWakeupReasonString();
