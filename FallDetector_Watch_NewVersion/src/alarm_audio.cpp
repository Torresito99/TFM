#include "alarm_audio.h"
#include "config.h"
#include <Wire.h>

// Patrón de pitidos
static const int      TONE_HZ     = 1000;   // frecuencia del pitido
static const int16_t  TONE_AMP    = 9000;   // amplitud (de 32767)
static const uint32_t BEEP_ON_MS  = 250;    // pitido encendido
static const uint32_t BEEP_OFF_MS = 150;    // silencio entre pitidos
static const int      CHUNK_FRAMES = AUDIO_SAMPLE_RATE * 30 / 1000;  // ~30 ms

// ---------------------------------------------------------------------
//  Registros I2C del ES8311
// ---------------------------------------------------------------------
void AlarmAudio::es8311WriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t AlarmAudio::es8311ReadReg(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Secuencia de init portada del driver oficial (esp-bsp/es8311.c).
// Configuración: MCLK desde el pin MCLK = 256·fs = 4.096 MHz, 16 kHz, 16 bits.
bool AlarmAudio::es8311Init() {
  // Comprueba que el códec responde (ACK en I2C)
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    Serial.println("[audio] ES8311 no responde en I2C 0x18");
    return false;
  }

  // Reset y power-on (modo esclavo)
  es8311WriteReg(0x00, 0x1F);  delay(20);
  es8311WriteReg(0x00, 0x00);
  es8311WriteReg(0x00, 0x80);

  // --- Clock manager (MCLK desde pin, no invertido) ---
  es8311WriteReg(0x01, 0x3F);
  uint8_t reg06 = es8311ReadReg(0x06);
  reg06 &= ~0x20;                       // SCLK no invertido
  es8311WriteReg(0x06, reg06);

  // --- Sample frequency (coeff de la tabla para 4.096MHz / 16kHz) ---
  uint8_t reg02 = es8311ReadReg(0x02);
  reg02 &= 0x07;                        // pre_div=1, pre_multi=0
  es8311WriteReg(0x02, reg02);
  es8311WriteReg(0x03, 0x10);           // fs_mode=0 | adc_osr=0x10
  es8311WriteReg(0x04, 0x10);           // dac_osr=0x10
  es8311WriteReg(0x05, 0x00);           // adc_div=1, dac_div=1
  reg06 = es8311ReadReg(0x06);
  reg06 &= 0xE0;
  reg06 |= (0x04 - 1);                  // bclk_div=4
  es8311WriteReg(0x06, reg06);
  uint8_t reg07 = es8311ReadReg(0x07);
  reg07 &= 0xC0;                        // lrck_h=0
  es8311WriteReg(0x07, reg07);
  es8311WriteReg(0x08, 0xFF);           // lrck_l=0xff

  // --- Formato I2S, 16 bits, esclavo ---
  uint8_t reg00 = es8311ReadReg(0x00);
  reg00 &= 0xBF;
  es8311WriteReg(0x00, reg00);
  es8311WriteReg(0x09, 0x0C);           // SDP in  = 16 bit
  es8311WriteReg(0x0A, 0x0C);           // SDP out = 16 bit

  // --- Encendido de la cadena de salida (DAC) ---
  es8311WriteReg(0x0D, 0x01);           // power up analog
  es8311WriteReg(0x0E, 0x02);           // enable PGA / modulador
  es8311WriteReg(0x12, 0x00);           // power up DAC
  es8311WriteReg(0x13, 0x10);           // habilita salida
  es8311WriteReg(0x1C, 0x6A);           // bypass eq ADC
  es8311WriteReg(0x37, 0x08);           // bypass eq DAC

  // Volumen alto y quitar mute
  es8311WriteReg(0x32, 0xE0);           // volumen DAC (~88%)
  es8311WriteReg(0x31, 0x00);           // unmute

  Serial.println("[audio] ES8311 inicializado");
  return true;
}

// ---------------------------------------------------------------------
bool AlarmAudio::begin() {
  pinMode(PA_ENABLE, OUTPUT);
  digitalWrite(PA_ENABLE, LOW);         // amplificador apagado de inicio

  // I2S estándar: BCLK, WS, DOUT, DIN, MCLK
  _i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
  if (!_i2s.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE,
                  I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[audio] Error al iniciar I2S");
    _ok = false;
    return false;
  }

  // El MCLK ya está corriendo: configuramos el códec
  _codecOk = es8311Init();
  _ok = true;                           // I2S ok aunque el códec falle (no bloquea)
  return _ok;
}

void AlarmAudio::start() {
  if (!_ok) return;
  _active   = true;
  _startMs  = millis();
  _sampleIdx = 0;
  digitalWrite(PA_ENABLE, HIGH);        // enciende el amplificador
}

void AlarmAudio::stop() {
  _active = false;
  // Un poco de silencio para vaciar el buffer y evitar "pop"
  static int16_t silence[CHUNK_FRAMES * 2] = {0};
  if (_ok) _i2s.write((uint8_t *)silence, sizeof(silence));
  digitalWrite(PA_ENABLE, LOW);         // apaga el amplificador
}

void AlarmAudio::beep(uint32_t ms) {
  if (!_ok) return;
  digitalWrite(PA_ENABLE, HIGH);
  delay(2);                                  // breve margen para el amplificador

  static int16_t buf[CHUNK_FRAMES * 2];
  const uint32_t halfPeriod = AUDIO_SAMPLE_RATE / (TONE_HZ * 2);
  uint32_t idx = 0;
  uint32_t start = millis();
  while (millis() - start < ms) {
    for (int f = 0; f < CHUNK_FRAMES; f++) {
      int16_t s = ((idx / halfPeriod) & 1) ? TONE_AMP : -TONE_AMP;
      idx++;
      buf[2 * f] = s; buf[2 * f + 1] = s;
    }
    _i2s.write((uint8_t *)buf, sizeof(buf));
  }

  static int16_t silence[CHUNK_FRAMES * 2] = {0};
  _i2s.write((uint8_t *)silence, sizeof(silence));
  digitalWrite(PA_ENABLE, LOW);
}

void AlarmAudio::playChunk() {
  if (!_ok || !_active) return;

  static int16_t buf[CHUNK_FRAMES * 2];   // estéreo (L,R)
  const uint32_t cycle = (millis() - _startMs) % (BEEP_ON_MS + BEEP_OFF_MS);
  const bool on = (cycle < BEEP_ON_MS);
  const uint32_t halfPeriod = AUDIO_SAMPLE_RATE / (TONE_HZ * 2);  // muestras por semiciclo

  for (int f = 0; f < CHUNK_FRAMES; f++) {
    int16_t s = 0;
    if (on) {
      s = ((_sampleIdx / halfPeriod) & 1) ? TONE_AMP : -TONE_AMP;  // onda cuadrada
    }
    _sampleIdx++;
    buf[2 * f]     = s;   // L
    buf[2 * f + 1] = s;   // R
  }

  _i2s.write((uint8_t *)buf, sizeof(buf));
}
