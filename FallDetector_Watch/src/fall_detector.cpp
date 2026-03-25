#include "fall_detector.h"

// Definir las constantes
const float FallDetector::FREE_FALL_THRESHOLD = 10.0;   // Menos de 10 m/s² = caída libre (MUY permisivo)
const float FallDetector::IMPACT_THRESHOLD = 5.0;       // Más de 5 m/s² = impacto (EXTREMADAMENTE bajo)
const int FallDetector::SAMPLE_COUNT = 10;              // Leer 10 muestras

void FallDetector::setupMotionDetection(TTGOClass *ttgo) {
    Serial.println("[FallDetector] Configurando detección de movimiento...");
    
    // 1. Encender el sensor
    ttgo->bma->begin();
    ttgo->bma->enableAccel();
    
    // 2. Activar la característica de "Wake-up" (Any-Motion)
    // Esto configura el BMA423 para detectar cualquier movimiento
    ttgo->bma->enableFeature(BMA423_WAKEUP, true);
    
    // 3. Habilitar la interrupción física hacia el pin INT
    ttgo->bma->enableWakeupInterrupt();
    
    Serial.println("[FallDetector] ✓ Sensor listo para detectar movimiento");
}

bool FallDetector::confirmFall(TTGOClass *ttgo) {
    Serial.println("[FallDetector] Confirmando si es caída...");
    
    float max_accel = 0.0;
    float min_accel = 100.0;
    
    // Leer múltiples muestras del acelerómetro
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        Accel acc;
        ttgo->bma->getAccel(acc);  // Pasar referencia al acelerador
        
        // Calcular magnitud de aceleración (raíz cuadrada de x² + y² + z²)
        float magnitude = sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
        
        Serial.printf("[FallDetector] Muestra %d: %.2f m/s²\n", i + 1, magnitude);
        
        if (magnitude > max_accel) max_accel = magnitude;
        if (magnitude < min_accel) min_accel = magnitude;
        
        delay(50); // 50ms entre muestras = 500ms totales
    }
    
    // Lógica de decisión:
    // - Si hay baja aceleración (caída libre) seguida de alto impacto = caída
    // - Si hay solo impacto alto = golpe normal (¡ignorar!)
    bool is_freefall = (min_accel < FREE_FALL_THRESHOLD);
    bool is_impact = (max_accel > IMPACT_THRESHOLD);
    
    Serial.printf("[FallDetector] Min: %.2f | Max: %.2f | Caída Libre: %s | Impacto: %s\n", 
                  min_accel, max_accel, is_freefall ? "SÍ" : "NO", is_impact ? "SÍ" : "NO");
    
    // Caída confirmada solo si hay AMBOS: caída libre + impacto
    bool is_fall = (is_freefall && is_impact);
    
    if (is_fall) {
        Serial.println("[FallDetector] ⚠️  CAÍDA CONFIRMADA");
    } else {
        Serial.println("[FallDetector] Falsa alarma - Movimiento normal");
    }
    
    return is_fall;
}

void FallDetector::debugPrintAccel(TTGOClass *ttgo) {
    Accel acc;
    ttgo->bma->getAccel(acc);  // Pasar referencia al acelerador
    float magnitude = sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
    
    Serial.printf("[DEBUG] X: %.2f | Y: %.2f | Z: %.2f | Mag: %.2f m/s²\n", 
                  acc.x, acc.y, acc.z, magnitude);
}
