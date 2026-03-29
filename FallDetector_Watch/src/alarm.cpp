/*******************************************************************************
 * @file    alarm.cpp
 * @brief   Sistema de alarma — vibración + tono por speaker I2S
 *
 * @details Usa el motor de vibración del T-Watch V3 con patrón on/off
 *          y genera un tono de sirena a través del speaker I2S (MAX98357A).
 ******************************************************************************/
#include "alarm.h"
#include "hardware.h"
#include "config.h"
#include <Arduino.h>
#include <driver/i2s.h>

/* ─── Definiciones de pines I2S del T-Watch V3 ────────────────────────────── */
static constexpr uint8_t I2S_BCK_PIN  = 26;
static constexpr uint8_t I2S_WS_PIN   = 25;
static constexpr uint8_t I2S_DOUT_PIN = 33;

static bool audioInitialized = false;

/* ─── Audio I2S ───────────────────────────────────────────────────────────── */

static void audioInit()
{
    // Encender amplificador de audio via API de la librería
    watch->enableAudio();
    delay(100);  // Dar tiempo al MAX98357A para estabilizarse

    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = I2S_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 128;
    i2s_config.tx_desc_auto_clear = true;
    i2s_config.use_apll = false;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num   = I2S_BCK_PIN;
    pin_config.ws_io_num    = I2S_WS_PIN;
    pin_config.data_out_num = I2S_DOUT_PIN;
    pin_config.data_in_num  = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[ALARM] Error i2s_driver_install: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[ALARM] Error i2s_set_pin: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);
        return;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    audioInitialized = true;
    Serial.println("[ALARM] Audio I2S inicializado OK");
}

static void audioDeinit()
{
    if (audioInitialized) {
        i2s_zero_dma_buffer(I2S_NUM_0);
        i2s_driver_uninstall(I2S_NUM_0);
        audioInitialized = false;
    }
    watch->disableAudio();
}

/// Genera un tono sinusoidal estéreo y lo envía por I2S al MAX98357A
void alarmPlayToneChunk(uint16_t freq, uint32_t duration_ms)
{
    if (!audioInitialized) return;

    const uint32_t totalFrames = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    const float phaseInc = 2.0f * M_PI * freq / I2S_SAMPLE_RATE;

    // Buffer estéreo: cada frame = 2 muestras (L + R)
    static constexpr uint16_t FRAMES_PER_BLOCK = 128;
    int16_t buf[FRAMES_PER_BLOCK * 2];  // *2 para estéreo
    float phase = 0.0f;
    uint32_t written = 0;

    while (written < totalFrames) {
        uint16_t frames = (totalFrames - written > FRAMES_PER_BLOCK)
                          ? FRAMES_PER_BLOCK
                          : (totalFrames - written);
        for (uint16_t i = 0; i < frames; i++) {
            int16_t sample = (int16_t)(sinf(phase) * 24000);  // Amplitud alta
            buf[i * 2]     = sample;  // Canal izquierdo
            buf[i * 2 + 1] = sample;  // Canal derecho
            phase += phaseInc;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t bytesWritten = 0;
        i2s_write(I2S_NUM_0, buf, frames * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        written += frames;
    }
}

/* ─── Motor de vibración ──────────────────────────────────────────────────── */

static void motorInit()
{
    watch->motor_begin();
    Serial.println("[ALARM] Motor de vibración inicializado");
}

static void motorOn()
{
    watch->motor->onec();
}

/* ─── API pública ─────────────────────────────────────────────────────────── */

void alarmInit()
{
    motorInit();
    audioInit();
    Serial.println("[ALARM] Sistema de alarma listo");
}

void alarmTrigger()
{
    Serial.println("[ALARM] === ALARMA ACTIVADA ===");

    uint32_t alarmStart = millis();
    bool vibState = false;
    uint32_t lastVibToggle = 0;
    bool toneHigh = true;
    uint32_t lastToneSwitch = 0;

    while ((millis() - alarmStart) < ALARM_DURATION_MS) {
        uint32_t now = millis();

        // Patrón de vibración: ON/OFF alternante
        uint32_t vibInterval = vibState ? VIBRATION_ON_MS : VIBRATION_OFF_MS;
        if ((now - lastVibToggle) >= vibInterval) {
            vibState = !vibState;
            if (vibState) {
                motorOn();
            }
            lastVibToggle = now;
        }

        // Sirena: alternar entre frecuencia alta y baja
        if ((now - lastToneSwitch) >= TONE_SWITCH_MS) {
            uint16_t freq = toneHigh ? ALARM_TONE_HI_HZ : ALARM_TONE_LO_HZ;
            alarmPlayToneChunk(freq, TONE_SWITCH_MS);
            toneHigh = !toneHigh;
            lastToneSwitch = millis();
        }

        yield();
    }

    alarmStop();
    Serial.println("[ALARM] === ALARMA FINALIZADA ===");
}

void alarmStop()
{
    audioDeinit();
}
