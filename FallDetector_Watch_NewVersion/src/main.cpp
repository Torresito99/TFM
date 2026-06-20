// =====================================================================
//  Detector de Caídas — ESP32-C6 2.06" Touch AMOLED (Waveshare)
//
//  PANTALLA NORMAL: hora (RTC PCF85063) + batería (PMU AXP2101).
//     Para ahorrar, la pantalla se apaga en reposo y se enciende al tocarla.
//
//  AL DETECTAR UNA CAÍDA:
//     1) Suena un pitido corto (aviso de cambio de modo).
//     2) PRE-ALARMA: cuenta atrás de 10 s "CAÍDA DETECTADA, la alarma va a
//        sonar". Durante estos 10 s se cancela con el BOTÓN o tocando la
//        PANTALLA (y no llega a sonar).
//     3) Si pasan los 10 s sin cancelar -> ALARMA: suena el pitido continuo,
//        pantalla "CAÍDA DETECTADA", y se EMITE una baliza BLE de aviso.
//        Aquí la alarma SOLO se para con el BOTÓN (la pantalla no la para).
//
//  Ahorro (Plan C): CPU a 80 MHz, radios apagadas salvo la baliza BLE,
//  IMU leído a ~100 Hz con micro-siestas (light sleep) entre muestras.
// =====================================================================
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <string.h>
#include "SensorQMI8658.hpp"
#include "SensorPCF85063.hpp"
#include "XPowersLib.h"

#include "config.h"
#include "fall_detector.h"
#include "ui.h"
#include "alarm_audio.h"
#include "ble_alert.h"
#if ENABLE_BATTLOG
#include "batt_log.h"
#endif

// ---------------------------------------------------------------------
//  Display CO5300 (QSPI)
// ---------------------------------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
    bus, LCD_RESET, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT,
    LCD_COL_OFFSET, 0, 0, 0);
Arduino_GFX *gfx = panel;

// ---------------------------------------------------------------------
//  Módulos
// ---------------------------------------------------------------------
SensorQMI8658  qmi;
SensorPCF85063 rtc;
XPowersAXP2101 pmu;
FallDetector   fd;
UI             ui;
AlarmAudio     alarmAudio;

enum class AppState { NORMAL, PRE_ALARM, ALARM, MSG_CANCELLED };
AppState state = AppState::NORMAL;

uint32_t preAlarmStart = 0;
uint32_t alarmStart    = 0;
uint32_t msgStart      = 0;
bool     imuOk = false, rtcOk = false, pmuOk = false;

// ¿Enchufado al USB? Si lo está, NO dormimos: el light sleep tumba el
// USB-CDC nativo del C6 (Windows pierde el COM y no se puede ni reflashear
// ni volcar el CSV). En batería sí dormimos -> medida de consumo real.
volatile bool gOnUsb = true;

// Pantalla
bool     displayOn    = true;
uint32_t lastActivity = 0;
int      lastMinute   = -1;
uint32_t lastBattMs   = 0;

// ---------------------------------------------------------------------
//  Botón BOOT con antirrebote (pulsado = LOW). Devuelve true en el flanco.
// ---------------------------------------------------------------------
bool buttonPressedEdge(uint32_t now) {
  static bool     lastStable = true;
  static bool     lastRead   = true;
  static uint32_t lastChange = 0;
  bool raw = digitalRead(BOOT_BUTTON);
  if (raw != lastRead) { lastRead = raw; lastChange = now; }
  if ((now - lastChange) > 30 && raw != lastStable) {
    lastStable = raw;
    if (lastStable == LOW) return true;
  }
  return false;
}

// ¿Dedo en la pantalla? Leemos el pin INT del FT3168 (sin I2C).
bool touchActive() {
  return digitalRead(TP_INT) == LOW;
}

// ---------------------------------------------------------------------
//  Pantalla y batería
// ---------------------------------------------------------------------
void wakeDisplay() {
  if (!displayOn) { panel->displayOn(); displayOn = true; }
}

void sleepDisplayIfIdle(uint32_t now) {
  if (displayOn && DISPLAY_TIMEOUT_MS > 0 &&
      (now - lastActivity) > DISPLAY_TIMEOUT_MS) {
    panel->displayOff();
    displayOn = false;
  }
}

int readBattery(bool &charging) {
  if (!pmuOk) { charging = false; return -1; }
  charging = pmu.isCharging();
  return pmu.getBatteryPercent();
}

// Dibuja la cara del reloj con la hora y batería actuales.
void drawClockNow() {
  int hh = 0, mm = 0;
  if (rtcOk) {
    RTC_DateTime dt = rtc.getDateTime();
    hh = dt.getHour(); mm = dt.getMinute();
    lastMinute = hh * 60 + mm;
  }
  bool ch; int p = readBattery(ch);
  ui.showClock(hh, mm, p, ch);
  lastBattMs = millis();
}

// Pone el RTC a la hora de compilación si no tiene una hora válida.
void setRtcIfNeeded() {
  if (!rtcOk) return;
  if (rtc.getDateTime().getYear() >= 2024) return;   // ya tiene hora
  char mon[4] = {0}; int d = 1, y = 2026, hh = 0, mm = 0, ss = 0;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
  static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strstr(months, mon);
  int mo = p ? ((int)(p - months) / 3 + 1) : 1;
  rtc.setDateTime(RTC_DateTime(y, mo, d, hh, mm, ss));
  Serial.printf("[rtc] hora puesta a %04d-%02d-%02d %02d:%02d\n", y, mo, d, hh, mm);
}

// Micro-siesta de la CPU entre muestras del IMU.
void powerNap(uint32_t ms) {
#if ENABLE_LIGHT_SLEEP
  // No dormir si: (a) enchufado al USB (el light sleep tumba el USB-CDC), o
  // (b) en los primeros BOOT_NOSLEEP_MS tras arrancar (ventana de seguridad
  // para que SIEMPRE se pueda reflashear/recuperar por USB).
  if (gOnUsb || millis() < BOOT_NOSLEEP_MS) { delay(ms); return; }
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
#else
  delay(ms);
#endif
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Detector de Caidas — ESP32-C6 AMOLED ===");

  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  Serial.printf("[pwr] CPU a %d MHz\n", getCpuFrequencyMhz());

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // Display
  if (!gfx->begin()) Serial.println("[gfx] begin() fallo!");
  panel->setBrightness(DISPLAY_BRIGHTNESS);
  gfx->fillScreen(RGB565_BLACK);

  // Táctil FT3168: reset + pin INT como entrada (lo leemos sin I2C)
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(10);
  digitalWrite(TP_RST, HIGH); delay(60);
  pinMode(TP_INT, INPUT_PULLUP);

  // IMU QMI8658 (~224 Hz en 6DOF)
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("[imu] QMI8658 NO encontrado!");
  } else {
    Serial.printf("[imu] QMI8658 OK, chip id 0x%X\n", qmi.getChipID());
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,
                            SensorQMI8658::ACC_ODR_250Hz,
                            SensorQMI8658::LPF_MODE_0);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_1024DPS,
                        SensorQMI8658::GYR_ODR_224_2Hz,
                        SensorQMI8658::LPF_MODE_3);
    qmi.enableAccelerometer();
    qmi.enableGyroscope();
    imuOk = true;
  }

  // RTC PCF85063
  rtcOk = rtc.begin(Wire, I2C_SDA, I2C_SCL);
  Serial.printf("[rtc] %s\n", rtcOk ? "OK" : "NO encontrado");
  setRtcIfNeeded();

  // PMU AXP2101 (batería)
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (pmuOk) {
    pmu.enableBattDetection();
    pmu.enableBattVoltageMeasure();
    Serial.println("[pmu] AXP2101 OK");
  } else {
    Serial.println("[pmu] AXP2101 NO encontrado");
  }

  fd.begin();
  ui.begin(gfx);

  alarmAudio.begin();
  Serial.printf("[audio] codec %s\n", alarmAudio.codecOk() ? "OK" : "NO disponible");

#if ENABLE_BATTLOG
  battLogBegin();
#endif

  drawClockNow();
  lastActivity = millis();
  Serial.println("Sistema listo. Monitorizando...");
}

// ---------------------------------------------------------------------
void loop() {
  const uint32_t now = millis();

  // Refresca el estado del USB (1 vez/seg). Decide si dormimos o no.
  static uint32_t lastUsbChk = 0;
  if (pmuOk && (now - lastUsbChk) >= 1000) {
    lastUsbChk = now;
    gOnUsb = pmu.isVbusIn();
  }

#if ENABLE_BATTLOG
  battLogHandleSerial();
  static uint32_t lastLog = 0;
  if ((now - lastLog) >= BATTLOG_INTERVAL_MS) {
    lastLog = now;
    bool ch; int p = readBattery(ch);
    int mv = pmuOk ? pmu.getBattVoltage() : 0;
    battLogRecord(now, p, mv, ch, gOnUsb);   // gOnUsb = ¿enchufado al USB?
  }
#endif

  switch (state) {

    // ----------------------------------------------------------- NORMAL
    case AppState::NORMAL: {
      // --- Detección de caídas (siempre, aunque la pantalla esté apagada) ---
      if (imuOk && qmi.getDataReady()) {
        float ax, ay, az, gx, gy, gz;
        qmi.getAccelerometer(ax, ay, az);
        qmi.getGyroscope(gx, gy, gz);

        if (fd.update(ax, ay, az, gx, gy, gz, now)) {
          Serial.printf("[CAIDA] impacto=%.2fg jerk=%.0f giro=%.0f preGiro=%.0f orient=%.0f score=%d\n",
                        fd.lastImpactG(), fd.lastJerk(), fd.lastGyroPeak(),
                        fd.lastPreGyro(), fd.lastOrient(), fd.lastScore());
          alarmAudio.beep(180);              // pitido de cambio de modo
          wakeDisplay();
          ui.showPreAlarm(PRE_ALARM_MS / 1000);
          preAlarmStart = now;
          state = AppState::PRE_ALARM;
          break;
        }
      }

      // --- Reloj + batería (solo si la pantalla está encendida) ---
      if (displayOn) {
        static uint32_t lastClockMs = 0;
        if (rtcOk && (now - lastClockMs) >= 1000) {
          lastClockMs = now;
          RTC_DateTime dt = rtc.getDateTime();
          int mn = dt.getHour() * 60 + dt.getMinute();
          if (mn != lastMinute) { lastMinute = mn; ui.updateTime(dt.getHour(), dt.getMinute()); }
        }
        if ((now - lastBattMs) >= BATTERY_REFRESH_MS) {
          lastBattMs = now;
          bool ch; int p = readBattery(ch);
          ui.updateBattery(p, ch);
        }
      }

      // Tocar la pantalla la enciende (y reinicia el temporizador)
      if (touchActive()) {
        if (!displayOn) { wakeDisplay(); drawClockNow(); }
        lastActivity = now;
      }
      sleepDisplayIfIdle(now);

      powerNap(SAMPLE_PERIOD_MS);
      break;
    }

    // -------------------------------------------------------- PRE-ALARMA
    //  Cuenta atrás de 10 s. Se cancela con BOTÓN o PANTALLA.
    case AppState::PRE_ALARM: {
      int32_t rem = (int32_t)PRE_ALARM_MS - (int32_t)(now - preAlarmStart);
      if (rem < 0) rem = 0;
      ui.updatePreAlarm((rem + 999) / 1000);

      if (buttonPressedEdge(now) || touchActive()) {
        Serial.println("[pre-alarma] cancelada (boton o pantalla)");
        state = AppState::MSG_CANCELLED;
        msgStart = now;
        ui.showCancelled();
      } else if ((now - preAlarmStart) >= PRE_ALARM_MS) {
        Serial.println("[ALARMA] sin respuesta -> suena alarma + baliza BLE");
        alarmAudio.start();
        bleAlertStart();                     // activa BT y emite la baliza
        ui.showAlarm();
        alarmStart = now;
        state = AppState::ALARM;
      }
      break;
    }

    // ------------------------------------------------------------ ALARMA
    //  Suena el pitido continuo + baliza BLE. SOLO se para con el BOTÓN.
    case AppState::ALARM: {
      alarmAudio.playChunk();
      ui.updateAlarm(((now / 400) % 2) == 0);

      if (buttonPressedEdge(now)) {          // la pantalla NO para la alarma
        Serial.println("[alarma] parada con boton BOOT");
        alarmAudio.stop();
        bleAlertStop();
        state = AppState::MSG_CANCELLED;
        msgStart = now;
        ui.showCancelled();
      }
      break;
    }

    // ------------------------------------------------------- CANCELADA
    case AppState::MSG_CANCELLED: {
      buttonPressedEdge(now);                // consume rebotes
      if ((now - msgStart) >= MSG_DURATION_MS) {
        fd.begin();
        displayOn = true;
        lastActivity = now;
        drawClockNow();
        state = AppState::NORMAL;
      }
      break;
    }
  }
}
