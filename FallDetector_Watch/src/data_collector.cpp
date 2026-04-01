/*******************************************************************************
 * @file    data_collector.cpp
 * @brief   Firmware V3 — Recolección de datos del acelerómetro (raw int16)
 *
 * @details Graba datos RAW del BMA423 en SPIFFS.
 *          - Almacena int16 directamente del sensor (sin conversión)
 *          - La conversión a g se hace en el PC al descargar
 *          - Cada 10s guarda automáticamente una ventana de 3s como "no-caída"
 *          - Serial "F" o botón lateral = marcar caída
 *          - Serial DOWNLOAD envía datos con la escala para que el PC convierta
 *
 *          Formato binario: 1 byte label + 150 × 3 × 2 bytes = 901 bytes/registro
 *
 * @hardware  LilyGO T-Watch V3 2020
 * @author    Torres
 ******************************************************************************/

#include <LilyGoWatch.h>
#include <SPIFFS.h>
#include <cmath>

using fs::File;

/* ─── Configuración ─────────────────────────────────────────────────────── */
static constexpr uint32_t SAMPLE_RATE_HZ     = 50;
static constexpr uint32_t SAMPLE_PERIOD_MS   = 1000 / SAMPLE_RATE_HZ;   // 20ms
static constexpr uint32_t WINDOW_SAMPLES     = 150;                      // 3s @ 50Hz
static constexpr uint32_t RING_SECONDS       = 10;
static constexpr uint32_t RING_SAMPLES       = SAMPLE_RATE_HZ * RING_SECONDS; // 500
static constexpr uint32_t NOFALL_SAVE_SEC    = 10;
static constexpr int16_t  LSB_PER_G          = 512;   // BMA423 ±4g, 12-bit
static constexpr float    LSB_TO_G           = 1.0f / LSB_PER_G;
static constexpr const char *DATA_PATH       = "/data.bin";

/* Cada registro: 1 byte label + 150 × 3 × 2 bytes raw int16 = 901 bytes */
static constexpr size_t RECORD_SIZE = 1 + WINDOW_SAMPLES * 3 * sizeof(int16_t);

/* ─── Hardware ──────────────────────────────────────────────────────────── */
static TTGOClass *watch = nullptr;
static TFT_eSPI  *tft   = nullptr;
static BMA       *bma   = nullptr;

/* ─── Ring buffer — almacena raw int16 (x, y, z) ──────────────────────── */
static int16_t  ringBuf[RING_SAMPLES][3];  // raw LSB values
static uint16_t ringPos       = 0;
static uint32_t totalSampled  = 0;

/* ─── Estado ────────────────────────────────────────────────────────────── */
static uint32_t totalWindows   = 0;
static uint32_t fallWindows    = 0;
static uint32_t nofallWindows  = 0;
static uint32_t lastAutoSave   = 0;
static uint32_t lastFallMark   = 0;
static bool     recording      = true;
static float    lastMag        = 0;
static float    maxMag         = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  SPIFFS: guardar ventana centrada en un índice del ring buffer
 * ═══════════════════════════════════════════════════════════════════════════ */
static bool saveWindowFromRing(uint8_t label, uint16_t centerIdx)
{
    int startIdx = (int)centerIdx - (int)(WINDOW_SAMPLES / 2);

    File f = SPIFFS.open(DATA_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("S,ERROR_SPIFFS_OPEN");
        return false;
    }

    f.write(&label, 1);

    for (uint16_t i = 0; i < WINDOW_SAMPLES; i++) {
        int idx = startIdx + (int)i;
        while (idx < 0)             idx += RING_SAMPLES;
        while (idx >= RING_SAMPLES) idx -= RING_SAMPLES;

        f.write((uint8_t *)ringBuf[idx], 6);  // 3 × int16 = 6 bytes
    }

    f.close();
    totalWindows++;
    if (label == 1) fallWindows++;
    else            nofallWindows++;
    return true;
}

/* Guardar las últimas 150 muestras */
static bool saveLatestWindow(uint8_t label)
{
    if (totalSampled < WINDOW_SAMPLES) return false;

    uint16_t endIdx = (ringPos == 0) ? RING_SAMPLES - 1 : ringPos - 1;
    int startIdx = (int)endIdx - (int)WINDOW_SAMPLES + 1;

    File f = SPIFFS.open(DATA_PATH, FILE_APPEND);
    if (!f) return false;

    f.write(&label, 1);

    for (uint16_t i = 0; i < WINDOW_SAMPLES; i++) {
        int idx = startIdx + (int)i;
        while (idx < 0)             idx += RING_SAMPLES;
        while (idx >= RING_SAMPLES) idx -= RING_SAMPLES;

        f.write((uint8_t *)ringBuf[idx], 6);
    }

    f.close();
    totalWindows++;
    if (label == 1) fallWindows++;
    else            nofallWindows++;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Buscar pico de aceleración en el ring buffer
 * ═══════════════════════════════════════════════════════════════════════════ */
static uint16_t findPeakIndex()
{
    int32_t best    = 0;
    uint16_t bestIdx = ringPos;

    uint32_t searchLen = (totalSampled < RING_SAMPLES) ? totalSampled : RING_SAMPLES;
    for (uint32_t i = 0; i < searchLen; i++) {
        uint16_t idx = (ringPos + RING_SAMPLES - searchLen + i) % RING_SAMPLES;
        // Usar suma de cuadrados (no necesita sqrt)
        int32_t mag2 = (int32_t)ringBuf[idx][0] * ringBuf[idx][0]
                     + (int32_t)ringBuf[idx][1] * ringBuf[idx][1]
                     + (int32_t)ringBuf[idx][2] * ringBuf[idx][2];
        if (mag2 > best) {
            best    = mag2;
            bestIdx = idx;
        }
    }
    return bestIdx;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Contar registros existentes al arrancar
 * ═══════════════════════════════════════════════════════════════════════════ */
static void countExistingRecords()
{
    if (!SPIFFS.exists(DATA_PATH)) return;

    File f = SPIFFS.open(DATA_PATH, FILE_READ);
    if (!f) return;

    size_t fileSize = f.size();
    uint32_t numRecords = fileSize / RECORD_SIZE;

    fallWindows = nofallWindows = 0;
    for (uint32_t i = 0; i < numRecords; i++) {
        f.seek(i * RECORD_SIZE);
        uint8_t label;
        if (f.read(&label, 1) == 1) {
            if (label == 1) fallWindows++;
            else            nofallWindows++;
        }
    }
    f.close();
    totalWindows = numRecords;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Marcar caída
 * ═══════════════════════════════════════════════════════════════════════════ */
static void markFall()
{
    uint32_t now = millis();
    if (totalSampled < WINDOW_SAMPLES) return;
    if (now - lastFallMark < 2000) return;  // debounce 2s

    uint16_t peakIdx = findPeakIndex();

    // Calcular pico en g para mostrar
    float px = ringBuf[peakIdx][0] * LSB_TO_G;
    float py = ringBuf[peakIdx][1] * LSB_TO_G;
    float pz = ringBuf[peakIdx][2] * LSB_TO_G;
    float peakG = sqrtf(px*px + py*py + pz*pz);

    if (saveWindowFromRing(1, peakIdx)) {
        lastFallMark = now;
        lastAutoSave = now;

        tft->fillScreen(TFT_RED);
        tft->setTextColor(TFT_WHITE, TFT_RED);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(3);
        tft->drawString("GUARDADO!", 120, 80);
        tft->setTextSize(2);
        char buf[32];
        snprintf(buf, sizeof(buf), "Caida #%lu", (unsigned long)fallWindows);
        tft->drawString(buf, 120, 125);
        snprintf(buf, sizeof(buf), "Pico: %.1f g", peakG);
        tft->drawString(buf, 120, 155);
        delay(1000);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Display principal
 * ═══════════════════════════════════════════════════════════════════════════ */
static void updateDisplay()
{
    tft->fillScreen(TFT_BLACK);

    tft->setTextDatum(TC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(recording ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
    tft->drawString(recording ? "GRABANDO - V3 RAW" : "PAUSADO", 120, 4);

    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[48];

    // Valores en vivo
    uint16_t lastIdx = (ringPos == 0) ? RING_SAMPLES - 1 : ringPos - 1;
    float lx = ringBuf[lastIdx][0] * LSB_TO_G;
    float ly = ringBuf[lastIdx][1] * LSB_TO_G;
    float lz = ringBuf[lastIdx][2] * LSB_TO_G;

    snprintf(buf, sizeof(buf), "X:%.2f Y:%.2f Z:%.2f", lx, ly, lz);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString(buf, 8, 20);

    snprintf(buf, sizeof(buf), "Mag: %.2fg  Max: %.2fg", lastMag, maxMag);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(buf, 8, 36);

    tft->setTextColor(TFT_RED, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Caidas: %lu", (unsigned long)fallWindows);
    tft->drawString(buf, 8, 56);

    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Normal: %lu", (unsigned long)nofallWindows);
    tft->drawString(buf, 130, 56);

    size_t used  = SPIFFS.usedBytes();
    size_t total = SPIFFS.totalBytes();
    uint8_t pct  = (uint8_t)(used * 100 / total);
    snprintf(buf, sizeof(buf), "SPIFFS: %u%%  (%luKB)", pct, (unsigned long)(used / 1024));
    tft->setTextColor(pct > 90 ? TFT_RED : TFT_DARKGREY, TFT_BLACK);
    tft->drawString(buf, 8, 72);

    tft->drawFastHLine(0, 90, 240, TFT_DARKGREY);

    /* Botón rojo */
    tft->fillRoundRect(10, 95, 220, 140, 12, TFT_RED);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->setTextSize(3);
    tft->drawString("MARCAR", 120, 145);
    tft->drawString("CAIDA", 120, 180);
    tft->setTextSize(1);
    tft->setTextColor(TFT_YELLOW, TFT_RED);
    tft->drawString("Serial 'F' o boton lateral", 120, 215);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Serial: DOWNLOAD / CLEAR / STATUS / F
 * ═══════════════════════════════════════════════════════════════════════════ */
static void handleSerial()
{
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "F" || cmd == "FALL") {
        markFall();
        updateDisplay();

    } else if (cmd == "DOWNLOAD") {
        bool wasRecording = recording;
        recording = false;

        if (!SPIFFS.exists(DATA_PATH)) {
            Serial.println("BEGIN,0,512");
            Serial.println("END");
            recording = wasRecording;
            return;
        }

        File f = SPIFFS.open(DATA_PATH, FILE_READ);
        uint32_t numRecords = f.size() / RECORD_SIZE;
        // Enviar escala LSB_PER_G para que el PC sepa cómo convertir
        Serial.printf("BEGIN,%lu,%d\n", (unsigned long)numRecords, LSB_PER_G);

        for (uint32_t r = 0; r < numRecords; r++) {
            uint8_t label;
            f.read(&label, 1);
            Serial.printf("R,%lu,%u\n", (unsigned long)r, label);

            for (uint16_t s = 0; s < WINDOW_SAMPLES; s++) {
                int16_t vals[3];
                f.read((uint8_t *)vals, 6);
                // Enviar raw int16 — la conversión la hace el PC
                Serial.printf("D,%d,%d,%d\n", vals[0], vals[1], vals[2]);
            }
        }

        f.close();
        Serial.println("END");
        recording = wasRecording;

    } else if (cmd == "CLEAR") {
        SPIFFS.remove(DATA_PATH);
        totalWindows = fallWindows = nofallWindows = 0;
        Serial.println("S,CLEARED");
        updateDisplay();

    } else if (cmd == "STATUS") {
        Serial.printf("STATUS,%lu,%lu,%lu,%lu,%lu\n",
                      (unsigned long)totalWindows,
                      (unsigned long)fallWindows,
                      (unsigned long)nofallWindows,
                      (unsigned long)SPIFFS.usedBytes(),
                      (unsigned long)SPIFFS.totalBytes());
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Setup
 * ═══════════════════════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);
    delay(100);

    watch = TTGOClass::getWatch();
    watch->begin();
    tft = watch->tft;
    bma = watch->bma;

    watch->openBL();
    watch->bl->adjust(100);

    /* Acelerómetro: configurar rango ±4g */
    bma->begin();
    bma->enableAccel();
    Acfg cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_4G;
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
    cfg.perf_mode = BMA4_CIC_AVG_MODE;
    bma->accelConfig(cfg);

    /* Botón lateral (AXP202 PEK) */
    watch->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    watch->power->clearIRQ();

    /* SPIFFS */
    if (!SPIFFS.begin(true)) {
        Serial.println("S,SPIFFS_FAIL");
        while (1) delay(1000);
    }

    countExistingRecords();
    lastAutoSave = millis();

    // Imprimir verificación de escala
    Accel raw;
    bma->getAccel(raw);
    Serial.printf("S,SCALE_CHECK raw.z=%d  -> %.2fg (debe ser ~1.0 quieto)\n",
                  raw.z, raw.z * LSB_TO_G);

    Serial.println("S,DATA_COLLECTOR_V3_RAW");
    Serial.printf("S,RECORDS,%lu,%lu,%lu\n",
                  (unsigned long)totalWindows,
                  (unsigned long)fallWindows,
                  (unsigned long)nofallWindows);

    updateDisplay();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Loop
 * ═══════════════════════════════════════════════════════════════════════════ */
void loop()
{
    static uint32_t nextSample        = 0;
    static uint32_t nextDisplayUpdate = 0;
    uint32_t now = millis();

    handleSerial();

    /* Botón lateral: marcar caída */
    watch->power->readIRQ();
    if (watch->power->isPEKShortPressIRQ()) {
        markFall();
        updateDisplay();
    }
    watch->power->clearIRQ();

    /* Muestrear acelerómetro a 50Hz */
    if (recording && now >= nextSample) {
        nextSample = now + SAMPLE_PERIOD_MS;

        Accel raw;
        bma->getAccel(raw);

        ringBuf[ringPos][0] = raw.x;
        ringBuf[ringPos][1] = raw.y;
        ringBuf[ringPos][2] = raw.z;
        ringPos = (ringPos + 1) % RING_SAMPLES;

        totalSampled++;

        // Magnitud en g (solo para display)
        float gx = raw.x * LSB_TO_G;
        float gy = raw.y * LSB_TO_G;
        float gz = raw.z * LSB_TO_G;
        lastMag = sqrtf(gx*gx + gy*gy + gz*gz);
        if (lastMag > maxMag) maxMag = lastMag;
    }

    /* Auto-guardar no-caída cada 10s */
    if (recording && totalSampled >= WINDOW_SAMPLES &&
        (now - lastAutoSave >= NOFALL_SAVE_SEC * 1000))
    {
        if (SPIFFS.usedBytes() < SPIFFS.totalBytes() * 95 / 100) {
            saveLatestWindow(0);
        }
        lastAutoSave = now;
    }

    /* Actualizar display cada segundo */
    if (now >= nextDisplayUpdate) {
        nextDisplayUpdate = now + 1000;
        updateDisplay();
    }

    yield();
}
