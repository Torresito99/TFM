/*******************************************************************************
 * @file    alarm.h
 * @brief   Sistema de alarma — vibración + tono sonoro por speaker I2S
 ******************************************************************************/
#pragma once

#include <cstdint>

/// Inicializa el motor de vibración y el sistema de audio
void alarmInit();

/// Ejecuta la alarma completa (vibración + sonido) durante ALARM_DURATION_MS
void alarmTrigger();

/// Reproduce un fragmento de tono por el speaker I2S
void alarmPlayToneChunk(uint16_t freq, uint32_t duration_ms);

/// Detiene la alarma y libera recursos de audio
void alarmStop();
