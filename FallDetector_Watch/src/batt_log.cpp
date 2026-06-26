#include "batt_log.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;

struct __attribute__((packed)) Sample {
  uint16_t minute;   // minutos desde el inicio de la prueba
  uint8_t  pct;      // batería % (255 = desconocido)
  uint16_t mv;       // voltaje en mV
};

static Sample   buf[BATTLOG_MAX];   // run en curso
static Sample   tmp[BATTLOG_MAX];   // auxiliar (mover/volcar)
static uint16_t curN = 0;
static uint32_t t0   = 0;
static bool     prevOnUsb = true;   // estado USB previo (para detectar la desconexión)

// Vuelca por serie (CSV) la prueba guardada como "anterior".
static void dumpPrev() {
  uint16_t n = prefs.getUShort("prevN", 0);
  if (n == 0) { Serial.println("[battlog] no hay prueba anterior guardada"); return; }
  size_t got = prefs.getBytes("prev", tmp, sizeof(tmp));
  uint16_t cnt = got / sizeof(Sample);
  if (cnt > n) cnt = n;

  Serial.println("===== CSV DURACION BATERIA (prueba anterior) =====");
  Serial.println("minuto,bateria_pct,voltaje_mV");
  for (uint16_t i = 0; i < cnt; i++) {
    int pct = (tmp[i].pct == 255) ? -1 : tmp[i].pct;
    Serial.printf("%u,%d,%u\n", tmp[i].minute, pct, tmp[i].mv);
  }
  if (cnt > 0) {
    Serial.printf("===== fin: %u muestras, duracion ~%u min (%.1f h) =====\n",
                  cnt, tmp[cnt - 1].minute, tmp[cnt - 1].minute / 60.0);
  }
}

void battLogBegin() {
  prefs.begin("battlog", false);

  // Mueve la prueba recién terminada ("cur") a "prev" para poder volcarla.
  uint16_t n = prefs.getUShort("curN", 0);
  if (n > 0) {
    size_t got = prefs.getBytes("cur", tmp, sizeof(tmp));
    prefs.putBytes("prev", tmp, got);
    prefs.putUShort("prevN", n);
  }

  // Empieza una prueba nueva
  curN = 0;
  t0   = millis();
  prefs.putUShort("curN", 0);

  Serial.println("\n[battlog] PRUEBA DE BATERIA ACTIVA (temporal)");
  dumpPrev();
  Serial.println("[battlog] comandos monitor: 'd'=volcar CSV anterior, 'r'=borrar guardado");
}

void battLogRecord(uint32_t nowMs, int pct, int mv, bool charging, bool onUsb) {
  // Mientras esté enchufado al USB no medimos: el cronómetro se mantiene a 0.
  if (onUsb) {
    t0 = nowMs;
    prevOnUsb = true;
    return;
  }

  // Justo al desconectar el USB -> arranca la medida desde el minuto 0.
  if (prevOnUsb) {
    curN = 0;
    t0   = nowMs;
    prefs.putUShort("curN", 0);
    if (Serial) Serial.println("[battlog] USB desconectado -> EMPIEZA la medida de bateria");
  }
  prevOnUsb = false;

  if (curN >= BATTLOG_MAX) return;            // log lleno
  buf[curN].minute = (nowMs - t0) / 60000UL;
  buf[curN].pct    = (pct < 0) ? 255 : (uint8_t)pct;
  buf[curN].mv     = (uint16_t)mv;
  curN++;

  prefs.putBytes("cur", buf, curN * sizeof(Sample));
  prefs.putUShort("curN", curN);

  if (Serial) {  // solo si hay PC conectado (sin coste si va con batería)
    Serial.printf("[battlog] min=%u bat=%d%% v=%dmV%s\n",
                  buf[curN - 1].minute, pct, mv, charging ? " (cargando)" : "");
  }
}

void battLogHandleSerial() {
  if (!Serial || !Serial.available()) return;
  char c = Serial.read();
  if (c == 'd') {
    dumpPrev();
  } else if (c == 'r') {
    prefs.remove("prev");
    prefs.putUShort("prevN", 0);
    Serial.println("[battlog] guardado 'prev' borrado");
  }
}
