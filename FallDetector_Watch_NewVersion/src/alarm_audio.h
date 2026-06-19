// =====================================================================
//  alarm_audio.h — Alarma sonora por códec ES8311 + I2S
//
//  El reloj no tiene buzzer: el sonido sale por un códec ES8311 que
//  alimenta un altavoz a través de un amplificador (enable en GPIO6).
//  El driver del ES8311 está portado del oficial de Espressif (esp-bsp).
// =====================================================================
#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

class AlarmAudio {
public:
  bool begin();        // inicializa I2S + ES8311 (deja el amplificador apagado)
  void start();        // enciende el amplificador y arranca el patrón de pitidos
  void stop();         // silencia y apaga el amplificador
  void playChunk();    // genera ~30 ms de sonido; llamar repetidamente mientras suena
  void beep(uint32_t ms);  // un único pitido (bloqueante) p. ej. al cambiar de modo
  bool codecOk() const { return _codecOk; }
  bool active() const  { return _active; }

private:
  I2SClass _i2s;
  bool _ok       = false;
  bool _codecOk  = false;
  bool _active   = false;
  uint32_t _startMs   = 0;
  uint32_t _sampleIdx = 0;

  bool    es8311Init();
  void    es8311WriteReg(uint8_t reg, uint8_t val);
  uint8_t es8311ReadReg(uint8_t reg);
};
