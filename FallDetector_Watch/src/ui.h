// =====================================================================
//  ui.h — Interfaz en la pantalla AMOLED (Arduino_GFX / CO5300)
//
//  Pantallas:
//   · RELOJ      — hora grande + % de batería (estado normal)
//   · PRE-ALARMA — cuenta atrás 10 s "se ha detectado caída" (cancelable
//                  con botón o pantalla, antes de que suene la alarma)
//   · ALARMA     — "CAÍDA DETECTADA", la alarma suena (solo para con botón)
// =====================================================================
#pragma once

#include <Arduino_GFX_Library.h>

class UI {
public:
  void begin(Arduino_GFX *gfx);

  // --- Reloj + batería ---
  void showClock(int hh, int mm, int battPct, bool charging);  // dibujo completo
  void updateTime(int hh, int mm);                             // solo la hora
  void updateBattery(int battPct, bool charging);              // solo la batería

  // --- Pre-alarma (cuenta atrás cancelable) ---
  void showPreAlarm(int secondsLeft);
  void updatePreAlarm(int secondsLeft);

  // --- Alarma sonando ---
  void showAlarm();
  void updateAlarm(bool flashOn);

  // --- Mensaje de cancelación ---
  void showCancelled();

private:
  Arduino_GFX *_gfx = nullptr;
  int _lastSeconds = -1;
  void centerText(const char *txt, uint8_t size, int16_t y, uint16_t color);
  void drawBattery(int battPct, bool charging);
};
