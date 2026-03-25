#include "bma_handler.h"

namespace BMAHandler {
    
void setupAccelerometer(TTGOClass *ttgo) {
    Serial.println("[BMAHandler] Inicializando BMA423...");
    
    // 1. Encender el sensor
    ttgo->bma->begin();
    ttgo->bma->enableAccel();
    
    // 2. Configurar detección de movimiento (Any Motion)
    // Esto hace que reaccione a CUALQUIER movimiento
    ttgo->bma->enableFeature(BMA423_WAKEUP, true);
    
    // 3. Configurar detección también con Activity
    // Algunos modelos necesitan ambas para ser más sensibles
    ttgo->bma->enableFeature(BMA423_ACTIVITY, true);
    
    // 4. Habilitar las interrupciones físicas
    ttgo->bma->enableWakeupInterrupt();
    ttgo->bma->enableActivityInterrupt();
    
    Serial.println("[BMAHandler] ✓ BMA423 inicializado correctamente");
    Serial.println("[BMAHandler] ✓ Configurado para detectar CUALQUIER movimiento");
}

void clearAccelerometerInterrupts(TTGOClass *ttgo) {
    // CRÍTICO: Limpiar el registro de interrupciones
    // Si no hacemos esto, el pin INT se queda ALTO indefinidamente
    // y el ESP32 no detectará futuros cambios (no despertará)
    
    bool interrupt_status;
    int attempts = 0;
    
    do {
        interrupt_status = ttgo->bma->readInterrupt();
        attempts++;
        
        if (attempts > 100) {
            Serial.println("[BMAHandler] ⚠️  Timeout limpiando interrupciones");
            break;
        }
    } while (interrupt_status != false);
    
    Serial.printf("[BMAHandler] ✓ Interrupciones limpias (%d intentos)\n", attempts);
}

bool isSensorOnline(TTGOClass *ttgo) {
    // Verificar que el sensor responde intentando leer
    // Si la operación es exitosa, el sensor está online
    try {
        Accel acc;
        ttgo->bma->getAccel(acc);
        Serial.println("[BMAHandler] ✓ Sensor online (BMA423 respondiendo)");
        return true;
    } catch (...) {
        Serial.println("[BMAHandler] ✗ Error: Sensor OFFLINE");
        return false;
    }
}

} // namespace BMAHandler
