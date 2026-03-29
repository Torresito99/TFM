/*******************************************************************************
 * @file    hardware.h
 * @brief   Inicialización del hardware del T-Watch V3
 ******************************************************************************/
#pragma once

#include <LilyGoWatch.h>

// Punteros globales al hardware del reloj
extern TTGOClass    *watch;
extern TFT_eSPI     *tft;
extern BMA           *bma;
extern AXP20X_Class *power;

/// Inicializa el hardware completo del T-Watch (PMU, display, periféricos)
void hardwareInit();

/// Inicializa solo lo mínimo para análisis post-wake (sin encender pantalla)
void hardwareInitMinimal();

/// Enciende la pantalla y configura display
void hardwareEnableDisplay();
