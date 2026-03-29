/*******************************************************************************
 * @file    display.h
 * @brief   Pantallas del sistema de detección de caídas
 ******************************************************************************/
#pragma once

#include "fall_detection.h"

/// Muestra pantalla de "analizando movimiento..." mientras se recogen muestras
void displayAnalyzing();

/// Muestra pantalla de caída detectada con los datos del análisis
void displayFallDetected(const FallResult &result);

/// Muestra pantalla de "sin caída" y cuenta atrás para volver a dormir
void displayNoCaida(const FallResult &result);

/// Muestra pantalla de alarma activa
void displayAlarm();
