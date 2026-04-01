/*******************************************************************************
 * @file    display.h
 * @brief   Pantallas del sistema de detección de caídas
 ******************************************************************************/
#pragma once

#include "fall_detection.h"

/// Muestra pantalla de caída detectada con los datos del análisis
void displayFallDetected(const FallResult &result);

/// Muestra pantalla de alarma activa
void displayAlarm();

/// Muestra pantalla de debug con resultado del análisis (siempre visible)
void displayDebug(const FallResult &result);
