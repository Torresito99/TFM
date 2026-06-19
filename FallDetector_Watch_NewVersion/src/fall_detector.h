// =====================================================================
//  fall_detector.h — Detección de caídas para reloj en la muñeca
//
//  Pensado para personas mayores y para reducir falsos positivos.
//  Una caída se confirma SOLO si ocurren las 3 fases en orden:
//
//    1) MOVIMIENTO PREVIO  — el brazo se mueve / cae (accel dinámica
//                            o giroscopio elevados justo antes).
//    2) IMPACTO            — pico brusco: magnitud alta + (jerk alto
//                            O giro alto). Un golpe a un reloj quieto
//                            NO tiene movimiento previo -> se descarta.
//    3) INMOVILIDAD        — tras el golpe, movimiento muy ligero
//                            sostenido (~1.5 s). Si sigue gesticulando
//                            o andando, NO salta.
//
//  Además, una puntuación (magnitud, jerk, giro, cambio de orientación)
//  confirma que el impacto es realmente de tipo caída.
// =====================================================================
#pragma once

#include <Arduino.h>

enum class FallPhase {
  MONITORING,   // vigilando, buscando un impacto válido
  POST_IMPACT   // hubo impacto, comprobando inmovilidad
};

class FallDetector {
public:
  void begin();

  // ax,ay,az en g ; gx,gy,gz en º/s ; nowMs = millis().
  // Devuelve true SOLO en el instante en que se confirma una caída.
  bool update(float ax, float ay, float az,
              float gx, float gy, float gz, uint32_t nowMs);

  FallPhase phase() const { return _phase; }

  // Diagnóstico de la última caída evaluada
  float lastImpactG()  const { return _impactPeakG; }
  float lastGyroPeak() const { return _impactGyro; }
  float lastJerk()     const { return _impactJerk; }
  float lastPreGyro()  const { return _preGyro; }
  float lastOrient()   const { return _orientDeg; }
  int   lastScore()    const { return _score; }

private:
  // Buffer circular para analizar la actividad PREVIA al impacto
  static const int BUF = 160;        // ~0.8 s a 200 Hz
  float    _bAct[BUF];
  float    _bGyr[BUF];
  uint32_t _bT[BUF];
  int      _bHead = 0;

  FallPhase _phase = FallPhase::MONITORING;

  // Orientación de referencia (gravedad filtrada en reposo)
  float _refX = 0, _refY = 0, _refZ = 1.0f;
  float _beforeX = 0, _beforeY = 0, _beforeZ = 1.0f;

  // Cálculo del jerk (derivada de la aceleración)
  float    _prevAmag = 1.0f;
  uint32_t _prevT    = 0;
  bool     _havePrev = false;

  // Registro del impacto en curso
  uint32_t _impactTime = 0;
  uint32_t _stillStart = 0;
  float _impactPeakG = 0, _impactGyro = 0, _impactJerk = 0;
  float _preGyro = 0, _preAct = 0;
  float _orientDeg = 0;
  int   _score = 0;

  void reset();
  void pushSample(float act, float gyr, uint32_t t);
  void preWindowMax(uint32_t now, float &gyrMax, float &actMax);
  static float angleBetween(float ax, float ay, float az,
                            float bx, float by, float bz);
};
