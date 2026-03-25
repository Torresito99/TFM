#pragma once
#include <Arduino.h>
#include <LilyGoWatch.h>

// Algoritmo simple de detección de caídas
class FallDetector {
public:
    // Configurar el algoritmo básico
    static void setupMotionDetection(TTGOClass *ttgo);
    
    // Algoritmo de confirmación: lee varios datos y decide si es caída
    static bool confirmFall(TTGOClass *ttgo);
    
    // Mostrar información en serial para debugging
    static void debugPrintAccel(TTGOClass *ttgo);

private:
    // Umbrales configurables (en m/s²)
    static const float FREE_FALL_THRESHOLD;      // Umbral de caída libre
    static const float IMPACT_THRESHOLD;         // Umbral de impacto
    static const int SAMPLE_COUNT;               // Número de muestras a leer
};
