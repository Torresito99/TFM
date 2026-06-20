#include "fall_detector.h"
#include "config.h"
#include <math.h>

void FallDetector::begin() {
  _refX = 0; _refY = 0; _refZ = 1.0f;
  _havePrev = false;
  _bHead = 0;
  for (int i = 0; i < BUF; i++) { _bAct[i] = 0; _bGyr[i] = 0; _bT[i] = 0; }
  reset();
}

void FallDetector::reset() {
  // Vuelve a vigilar. NO toca la orientación de referencia ni el buffer.
  _phase = FallPhase::MONITORING;
  _impactTime = 0;
  _stillStart = 0;
  _impactPeakG = 0;
  _impactGyro = 0;
  _impactJerk = 0;
  _preGyro = 0;
  _preAct = 0;
}

void FallDetector::pushSample(float act, float gyr, uint32_t t) {
  _bAct[_bHead] = act;
  _bGyr[_bHead] = gyr;
  _bT[_bHead]   = t;
  _bHead = (_bHead + 1) % BUF;
}

// Máximos de actividad y giro en la ventana [now-PRE_WINDOW, now-30ms].
// Se excluyen los últimos 30 ms para no contar el propio impacto.
void FallDetector::preWindowMax(uint32_t now, float &gyrMax, float &actMax) {
  gyrMax = 0; actMax = 0;
  uint32_t lo = (now > PRE_WINDOW_MS) ? (now - PRE_WINDOW_MS) : 0;
  uint32_t hi = (now > 30) ? (now - 30) : 0;
  for (int i = 0; i < BUF; i++) {
    if (_bT[i] == 0) continue;
    if (_bT[i] >= lo && _bT[i] <= hi) {
      if (_bGyr[i] > gyrMax) gyrMax = _bGyr[i];
      if (_bAct[i] > actMax) actMax = _bAct[i];
    }
  }
}

float FallDetector::angleBetween(float ax, float ay, float az,
                                 float bx, float by, float bz) {
  float na = sqrtf(ax*ax + ay*ay + az*az);
  float nb = sqrtf(bx*bx + by*by + bz*bz);
  if (na < 1e-3f || nb < 1e-3f) return 0;
  float dot = (ax*bx + ay*by + az*bz) / (na * nb);
  if (dot > 1.0f)  dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * 57.2957795f;
}

bool FallDetector::update(float ax, float ay, float az,
                          float gx, float gy, float gz, uint32_t nowMs) {
  const float aMag = sqrtf(ax*ax + ay*ay + az*az);   // módulo aceleración (g)
  const float aDyn = fabsf(aMag - 1.0f);             // parte dinámica (sin gravedad)
  const float gMag = sqrtf(gx*gx + gy*gy + gz*gz);   // módulo giro (dps)

  // Jerk = variación de la aceleración (g/s)
  float jerk = 0;
  if (_havePrev) {
    float dt = (nowMs - _prevT) * 0.001f;
    if (dt > 0) jerk = fabsf(aMag - _prevAmag) / dt;
  }
  _prevAmag = aMag;
  _prevT    = nowMs;
  _havePrev = true;

  bool fall = false;

  switch (_phase) {

    // -----------------------------------------------------------------
    case FallPhase::MONITORING: {
      // Actualiza la orientación de referencia solo cuando está en calma
      if (aDyn < STILL_ACT && gMag < STILL_GYR) {
        _refX = _refX * 0.95f + ax * 0.05f;
        _refY = _refY * 0.95f + ay * 0.05f;
        _refZ = _refZ * 0.95f + az * 0.05f;
      }

      // ¿Impacto? magnitud alta Y (jerk alto O giro alto)
      const bool impactMag = (aMag >= IMPACT_G);
      const bool sharp     = (jerk >= JERK_THRESH) || (gMag >= GYR_IMPACT);

      if (impactMag && sharp) {
        // ¿Hubo movimiento ANTES del golpe? (descarta golpes a reloj quieto)
        float pg, pa;
        preWindowMax(nowMs, pg, pa);
        const bool wasMoving = (pg >= PRE_GYRO_MIN) || (pa >= PRE_ACT_MIN);

        if (wasMoving) {
          _phase = FallPhase::POST_IMPACT;
          _impactTime  = nowMs;
          _stillStart  = 0;
          _impactPeakG = aMag;
          _impactGyro  = gMag;
          _impactJerk  = jerk;
          _preGyro     = pg;
          _preAct      = pa;
          _beforeX = _refX; _beforeY = _refY; _beforeZ = _refZ;
        }
        // si no había movimiento previo -> golpe aislado, se ignora
      }
      break;
    }

    // -----------------------------------------------------------------
    case FallPhase::POST_IMPACT: {
      // Sigue capturando los picos un poco después del golpe
      if (aMag > _impactPeakG) _impactPeakG = aMag;
      if (gMag > _impactGyro)  _impactGyro  = gMag;
      if (jerk > _impactJerk)  _impactJerk  = jerk;

      const bool stillNow = (aDyn < STILL_ACT) && (gMag < STILL_GYR);

      if (nowMs - _impactTime < SETTLE_MS) {
        // Ventana inicial: solo capturamos picos, aún no medimos quietud
        _stillStart = 0;
      } else if (stillNow) {
        if (_stillStart == 0) {
          _stillStart = nowMs;
        } else if (nowMs - _stillStart >= STILL_MS) {
          // Inmóvil el tiempo suficiente -> evaluar puntuación
          const float ang = angleBetween(_beforeX, _beforeY, _beforeZ, ax, ay, az);
          _orientDeg = ang;

          int score = 0;
          if (_impactPeakG >= IMPACT_G)         score++;
          if (_impactPeakG >= IMPACT_HARD_G)    score++;
          if (_impactJerk  >= JERK_STRONG)      score++;
          if (_impactGyro  >= GYR_IMPACT)       score++;
          if (_impactGyro  >= GYR_IMPACT_STRONG) score++;
          if (ang >= ORIENT_CHANGE_DEG)         score++;
          if (ang >= ORIENT_CHANGE_BIG)         score++;
          _score = score;

          fall = (score >= SCORE_THRESHOLD);
          reset();
        }
      } else {
        // Se está moviendo: reinicia el contador de inmovilidad
        _stillStart = 0;
        if (nowMs - _impactTime > POST_TIMEOUT_MS) {
          reset();   // sigue activo tras el golpe -> probablemente está bien
        }
      }
      break;
    }
  }

  // Guarda la muestra para el análisis de "movimiento previo"
  pushSample(aDyn, gMag, nowMs);
  return fall;
}
