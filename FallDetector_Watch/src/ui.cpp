#include "ui.h"
#include "config.h"

// Colores
#define COL_BG_DARK   RGB565(8, 10, 14)
#define COL_GREEN     RGB565(0, 220, 90)
#define COL_RED       RGB565(230, 20, 20)
#define COL_RED_DK    RGB565(60, 0, 0)
#define COL_ORANGE    RGB565(255, 140, 0)
#define COL_YELLOW    RGB565(240, 210, 0)
#define COL_WHITE     RGB565_WHITE
#define COL_GREY      RGB565(150, 160, 170)

// Zonas de pantalla (410 x 502)
#define TIME_Y    170
#define BATT_Y    360

void UI::begin(Arduino_GFX *gfx) {
  _gfx = gfx;
}

void UI::centerText(const char *txt, uint8_t size, int16_t y, uint16_t color) {
  _gfx->setTextSize(size);
  _gfx->setTextColor(color);
  int16_t x1, y1; uint16_t w, h;
  _gfx->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (LCD_WIDTH - (int16_t)w) / 2;
  _gfx->setCursor(x, y);
  _gfx->print(txt);
}

// ---------------------------------------------------------------- RELOJ
void UI::showClock(int hh, int mm, int battPct, bool charging) {
  _gfx->fillScreen(COL_BG_DARK);
  centerText("Detector activo", 2, 70, COL_GREEN);
  updateTime(hh, mm);
  updateBattery(battPct, charging);
}

void UI::updateTime(int hh, int mm) {
  if (!_gfx) return;
  _gfx->fillRect(0, TIME_Y, LCD_WIDTH, 90, COL_BG_DARK);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
  centerText(buf, 9, TIME_Y, COL_WHITE);
}

void UI::drawBattery(int battPct, bool charging) {
  // Color según nivel
  uint16_t col = COL_GREEN;
  if (battPct < 0)      col = COL_GREY;
  else if (battPct <= 15) col = COL_RED;
  else if (battPct <= 35) col = COL_YELLOW;
  if (charging)         col = COL_GREEN;

  // Dibujo de pila + texto
  const int bw = 90, bh = 42, bx = (LCD_WIDTH - bw) / 2 - 30, by = BATT_Y;
  _gfx->drawRoundRect(bx, by, bw, bh, 4, col);
  _gfx->fillRect(bx + bw, by + 12, 6, bh - 24, col);           // borne
  if (battPct >= 0) {
    int fillw = (bw - 6) * battPct / 100;
    _gfx->fillRect(bx + 3, by + 3, fillw, bh - 6, col);
  }

  char buf[12];
  if (battPct < 0) snprintf(buf, sizeof(buf), "--%%");
  else             snprintf(buf, sizeof(buf), "%d%%%s", battPct, charging ? "+" : "");
  _gfx->setTextSize(3);
  _gfx->setTextColor(col);
  _gfx->setCursor(bx + bw + 18, by + 9);
  _gfx->print(buf);
}

void UI::updateBattery(int battPct, bool charging) {
  if (!_gfx) return;
  _gfx->fillRect(0, BATT_Y - 4, LCD_WIDTH, 60, COL_BG_DARK);
  drawBattery(battPct, charging);
}

// ------------------------------------------------------------ PRE-ALARMA
void UI::showPreAlarm(int secondsLeft) {
  _lastSeconds = -1;
  _gfx->fillScreen(COL_ORANGE);
  centerText("CAIDA", 6, 50, COL_WHITE);
  centerText("DETECTADA", 4, 130, COL_WHITE);
  centerText("La alarma sonara en", 2, 200, COL_WHITE);
  centerText("BOTON o PANTALLA = cancelar", 2, 455, COL_WHITE);
  updatePreAlarm(secondsLeft);
}

void UI::updatePreAlarm(int secondsLeft) {
  if (!_gfx) return;
  if (secondsLeft == _lastSeconds) return;
  _lastSeconds = secondsLeft;
  _gfx->fillRect(LCD_WIDTH / 2 - 80, 250, 160, 150, COL_ORANGE);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", secondsLeft);
  centerText(buf, 11, 255, COL_WHITE);
}

// --------------------------------------------------------------- ALARMA
void UI::showAlarm() {
  _lastSeconds = -1;
  _gfx->fillScreen(COL_RED);
  centerText("!CAIDA!", 6, 110, COL_WHITE);
  centerText("DETECTADA", 5, 200, COL_WHITE);
  centerText("ALARMA ACTIVA", 3, 300, COL_WHITE);
  centerText("Pulsa BOTON para parar", 2, 450, COL_WHITE);
}

void UI::updateAlarm(bool flashOn) {
  if (!_gfx) return;
  uint16_t frame = flashOn ? COL_WHITE : COL_RED;
  for (int i = 0; i < 6; i++) {
    _gfx->drawRect(i, i, LCD_WIDTH - 2 * i, LCD_HEIGHT - 2 * i, frame);
  }
}

// ------------------------------------------------------------ CANCELADA
void UI::showCancelled() {
  _lastSeconds = -1;
  _gfx->fillScreen(RGB565(0, 90, 40));
  centerText("ALARMA", 5, 180, COL_WHITE);
  centerText("CANCELADA", 4, 270, COL_WHITE);
}
