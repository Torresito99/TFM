#pragma once
#include <LilyGoWatch.h>

// Funciones de bajo nivel para interactuar con el BMA423
namespace BMAHandler {
    // Configurar el acelerómetro con parámetros específicos
    void setupAccelerometer(TTGOClass *ttgo);
    
    // Limpiar interrupciones pendientes (CRÍTICO antes de deep sleep)
    void clearAccelerometerInterrupts(TTGOClass *ttgo);
    
    // Verificar si el sensor está respondiendo
    bool isSensorOnline(TTGOClass *ttgo);
};
