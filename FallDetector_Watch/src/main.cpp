#define LILYGO_WATCH_2020_V3 
#include <Arduino.h>
#include <LilyGoWatch.h>

TTGOClass *ttgo;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Esperar a que Serial se estabilice
    
    Serial.println("\n\n════════════════════════════════");
    Serial.println("PRUEBA SIMPLE DE COMUNICACIÓN");
    Serial.println("════════════════════════════════\n");
    
    // 1. Inicializar reloj
    Serial.println("[1] Inicializando TTGO Watch...");
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    Serial.println("[✓] TTGO inicializado\n");
    
    // 2. Inicializar pantalla
    Serial.println("[2] Inicializando pantalla...");
    ttgo->openBL();
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    ttgo->tft->setCursor(10, 50);
    ttgo->tft->print("PRUEBA");
    Serial.println("[✓] Pantalla OK\n");
    
    // 3. Inicializar BMA423
    Serial.println("[3] Inicializando BMA423...");
    ttgo->bma->begin();
    ttgo->bma->enableAccel();
    Serial.println("[✓] BMA423 inicio OK\n");
    
    Serial.println("════════════════════════════════");
    Serial.println("INICIANDO LECTURA DE SENSOR");
    Serial.println("════════════════════════════════\n");
}

void loop() {
    // Leer continuamente el sensor
    delay(500);
    
    Accel acc;
    bool result = ttgo->bma->getAccel(acc);
    
    if (!result) {
        Serial.println("ERROR: No se pudo leer del sensor");
        return;
    }
    
    // Verificar valores válidos
    float x = isnan(acc.x) ? 0.0 : acc.x;
    float y = isnan(acc.y) ? 0.0 : acc.y;
    float z = isnan(acc.z) ? 0.0 : acc.z;
    
    // Mostrar en Serial cada 500ms
    Serial.printf("X: %7.2f | Y: %7.2f | Z: %7.2f m/s²\n", x, y, z);
    
    // Mostrar en pantalla
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE);
    ttgo->tft->setTextSize(2);
    
    ttgo->tft->setCursor(10, 20);
    ttgo->tft->printf("X: %.1f", x);
    
    ttgo->tft->setCursor(10, 50);
    ttgo->tft->printf("Y: %.1f", y);
    
    ttgo->tft->setCursor(10, 80);
    ttgo->tft->printf("Z: %.1f", z);
}
