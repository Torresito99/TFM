/*******************************************************************************
 * @file    main.cpp
 * @brief   LilyGO T-Watch V3 2020 - Deep Sleep con Wake-up por movimiento
 *
 * @details El dispositivo entra en Deep Sleep y se despierta cuando el
 *          acelerómetro BMA423 detecta movimiento (any-motion / wrist tilt).
 *          Base para un futuro sistema de detección de caídas.
 *
 * @hardware  LilyGO T-Watch V3 2020
 * @sensor    BMA423 (acelerómetro integrado)
 * @author    Tu nombre
 * @date      2025
 ******************************************************************************/

#include <LilyGoWatch.h>

/* ─── Objetos globales ─────────────────────────────────────────────────────── */
TTGOClass   *watch  = nullptr;
TFT_eSPI    *tft    = nullptr;
BMA          *bma   = nullptr;
AXP20X_Class *power = nullptr;

/* ─── Configuración ────────────────────────────────────────────────────────── */
static constexpr uint32_t AWAKE_TIME_MS        = 5000;   // Tiempo despierto (ms)
static constexpr uint8_t  BMA423_INT_PIN       = 39;     // GPIO del BMA423 INT
static constexpr uint8_t  BACKLIGHT_LEVEL      = 128;    // Brillo pantalla (0-255)

/* ─── Contadores persistentes en RTC memory ────────────────────────────────── */
RTC_DATA_ATTR uint32_t wakeupCount = 0;   // Sobrevive al deep sleep

/* ─── Prototipos ───────────────────────────────────────────────────────────── */
void initHardware();
void initAccelerometer();
void configureWakeupInterrupt();
void showWakeupScreen();
void prepareDeepSleep();
void enterDeepSleep();
const char *getWakeupReasonString();

/*******************************************************************************
 * @brief   Setup principal
 ******************************************************************************/
void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n========================================");
    Serial.println("  T-Watch V3 - Motion Wake-up System");
    Serial.println("========================================");

    // Incrementar contador de wake-ups
    wakeupCount++;
    Serial.printf("[BOOT] Wake-up #%u | Razón: %s\n", wakeupCount, getWakeupReasonString());

    // Inicializar hardware
    initHardware();

    // Configurar acelerómetro BMA423
    initAccelerometer();

    // Configurar interrupción de wake-up
    configureWakeupInterrupt();

    // Mostrar información en pantalla
    showWakeupScreen();

    // Esperar un tiempo antes de dormir
    Serial.printf("[MAIN] Permaneciendo despierto %u ms...\n", AWAKE_TIME_MS);
    delay(AWAKE_TIME_MS);

    // Entrar en deep sleep
    enterDeepSleep();
}

/*******************************************************************************
 * @brief   Loop (no se alcanza en este diseño)
 ******************************************************************************/
void loop()
{
    // El flujo es: setup → deep sleep → wake-up → setup
    // Este loop nunca se ejecuta en la lógica actual
}

/*******************************************************************************
 * @brief   Inicializa el hardware del T-Watch (pantalla, power, etc.)
 ******************************************************************************/
void initHardware()
{
    Serial.println("[HW] Inicializando hardware...");

    // Obtener instancia del watch
    watch = TTGOClass::getWatch();
    watch->begin();

    // Referencias a periféricos
    tft   = watch->tft;
    power = watch->power;
    bma   = watch->bma;

    // Encender pantalla
    watch->openBL();
    watch->bl->adjust(BACKLIGHT_LEVEL);

    // Configurar pantalla
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(MC_DATUM);

    Serial.println("[HW] Hardware inicializado correctamente");
}

/*******************************************************************************
 * @brief   Configura el acelerómetro BMA423 para detectar movimiento
 ******************************************************************************/
void initAccelerometer()
{
    Serial.println("[BMA] Configurando acelerómetro BMA423...");

    // Resetear el sensor
    bma->begin();
    bma->attachInterrupt();

    /*
     * Activar funciones del BMA423:
     *   - BMA423_WAKEUP:    Detecta movimiento brusco (any-motion)
     *   - BMA423_TILT:      Detecta inclinación de muñeca
     *   - BMA423_ANY_MOTION: Detecta cualquier movimiento
     *
     * Para detección de caídas futura, BMA423_WAKEUP es un buen punto de partida.
     * Se puede combinar con BMA423_ANY_MOTION para mayor sensibilidad.
     */

    // Habilitar la función de wakeup (any-motion basado)
    bma->enableAccel();

    // Activar feature: wakeup por movimiento
    bma->enableFeature(BMA423_WAKEUP, true);

    // Opcional: activar también tilt (inclinación de muñeca)
    bma->enableFeature(BMA423_TILT, true);

    // Mapear las interrupciones al pin INT del BMA423
    bma->enableWakeupInterrupt(true);
    bma->enableTiltInterrupt(true);

    Serial.println("[BMA] Acelerómetro configurado:");
    Serial.println("      - Wakeup (any-motion): ACTIVADO");
    Serial.println("      - Tilt (inclinación):  ACTIVADO");
}

/*******************************************************************************
 * @brief   Configura el ESP32 para despertar con la interrupción del BMA423
 ******************************************************************************/
void configureWakeupInterrupt()
{
    Serial.printf("[WAKE] Configurando GPIO %d como fuente de wake-up...\n", BMA423_INT_PIN);

    /*
     * El BMA423 del T-Watch V3 está conectado al GPIO 39 (INT).
     * Este pin genera un pulso HIGH cuando se dispara la interrupción.
     *
     * esp_sleep_enable_ext0_wakeup:
     *   - GPIO 39
     *   - Nivel HIGH (1) para despertar
     */
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BMA423_INT_PIN, 1);

    Serial.println("[WAKE] Fuente de wake-up configurada correctamente");
}

/*******************************************************************************
 * @brief   Muestra información del wake-up en la pantalla TFT
 ******************************************************************************/
void showWakeupScreen()
{
    tft->fillScreen(TFT_BLACK);

    // Título
    tft->setTextSize(2);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("MOTION DETECTED", 120, 30);

    // Separador
    tft->drawFastHLine(20, 55, 200, TFT_CYAN);

    // Información
    tft->setTextSize(1);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);

    tft->drawString("Wake-up reason:", 120, 80);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->drawString(getWakeupReasonString(), 120, 100);

    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("Wake-up count:", 120, 130);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);

    char countStr[32];
    snprintf(countStr, sizeof(countStr), "#%u", wakeupCount);
    tft->drawString(countStr, 120, 150);

    // Estado
    tft->setTextColor(TFT_ORANGE, TFT_BLACK);

    char sleepMsg[64];
    snprintf(sleepMsg, sizeof(sleepMsg), "Sleep in %u s...", AWAKE_TIME_MS / 1000);
    tft->drawString(sleepMsg, 120, 190);

    // Footer
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->drawString("Move wrist to wake up", 120, 225);
}

/*******************************************************************************
 * @brief   Prepara los periféricos para el bajo consumo antes de dormir
 ******************************************************************************/
void prepareDeepSleep()
{
    Serial.println("[SLEEP] Preparando periféricos para deep sleep...");

    // Apagar pantalla
    watch->closeBL();
    watch->displaySleep();

    /*
     * Apagar periféricos innecesarios via AXP202 (PMU).
     * Mantener encendido solo lo esencial:
     *   - LDO1: RTC (siempre encendido)
     *   - LDO3: Alimentación del BMA423 (NECESARIO para wake-up)
     *
     * Apagar:
     *   - LDO2: Backlight de la pantalla
     *   - LDO4: Audio
     *   - DC-DC2: No usado
     */

    // Apagar backlight
    power->setPowerOutPut(AXP202_LDO2, false);

    // Apagar audio (si aplica)
    power->setPowerOutPut(AXP202_LDO4, false);

    // IMPORTANTE: NO apagar LDO3 si el BMA423 lo necesita para la interrupción
    // En el V3, el BMA423 se alimenta típicamente de LDO3 o directamente del 3V3

    Serial.println("[SLEEP] Periféricos apagados");
}

/*******************************************************************************
 * @brief   Entra en modo Deep Sleep del ESP32
 ******************************************************************************/
void enterDeepSleep()
{
    Serial.println("[SLEEP] ================================");
    Serial.println("[SLEEP] Entrando en DEEP SLEEP...");
    Serial.println("[SLEEP] Mueve el reloj para despertar");
    Serial.println("[SLEEP] ================================\n");
    Serial.flush();

    // Preparar periféricos para bajo consumo
    prepareDeepSleep();

    // Pequeña pausa para asegurar que todo se apagó correctamente
    delay(50);

    // Entrar en deep sleep
    esp_deep_sleep_start();

    // ──── Nunca se llega aquí ────
}

/*******************************************************************************
 * @brief   Retorna la razón del último wake-up como string legible
 * @return  Cadena con la descripción de la razón
 ******************************************************************************/
const char *getWakeupReasonString()
{
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

    switch (reason)
    {
        case ESP_SLEEP_WAKEUP_EXT0:
            return "EXT0 (BMA423 Motion)";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "EXT1 (External)";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP Coprocessor";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            return "Power ON / Reset";
    }
}