#include "power_manager.h"

void PowerManager::init(TTGOClass *ttgo) {
    // Iniciar el chip de energía
    ttgo->power->begin();
    keepSensorPowered(ttgo);
}

void PowerManager::keepSensorPowered(TTGOClass *ttgo) {
    // OBLIGATORIO: Mantener encendido el sensor aunque el reloj duerma
    // En el V3, el sensor de movimiento y el bus I2C se alimentan de LDO3 y LDO4
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_ON);
    ttgo->power->setPowerOutPut(AXP202_LDO4, AXP202_ON);
}

void PowerManager::goToDeepSleep(TTGOClass *ttgo) {
    Serial.println("[PowerManager] Entrando en Deep Sleep. Sensor vigilando...");
    
    // 1. Apagamos la pantalla para ahorrar energía
    ttgo->closeBL();
    ttgo->displaySleep();

    // 2. CRÍTICO: Limpiar interrupciones pendientes del sensor
    // Si no hacemos esto, el pin 39 se queda en ALTO y no detecta cambios
    bool interrupt_pending;
    do {
        interrupt_pending = ttgo->bma->readInterrupt();
    } while (interrupt_pending != false);

    Serial.println("[PowerManager] Interrupciones limpias. Configurando wake-up...");

    // 3. Configurar despertar con el pin 39 (BMA INT pin)
    // ext0: Despierta cuando el pin reciba un nivel ALTO (1)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BMA_INT_PIN, 1);

    // 4. ¡A dormir!
    esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t PowerManager::getWakeupReason() {
    return esp_sleep_get_wakeup_cause();
}
