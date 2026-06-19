// =====================================================================
//  batt_log.h — Registro de duración de batería  (TEMPORAL, solo pruebas)
//
//  Guarda en la flash (NVS) muestras de {minuto, batería%, voltaje} para
//  medir cuánto dura el reloj SIN estar conectado al PC. El registro
//  sobrevive a que se agote la batería: al reconectar y arrancar, la
//  prueba recién terminada queda guardada como "anterior" y se puede
//  volcar como CSV escribiendo 'd' en el monitor serie.
//
//  Para quitarlo del proyecto: poner ENABLE_BATTLOG a 0 en config.h.
// =====================================================================
#pragma once
#include <Arduino.h>

void battLogBegin();   // archiva la prueba anterior y prepara una nueva
// La medida arranca sola cuando se desconecta el USB (onUsb pasa a false).
void battLogRecord(uint32_t nowMs, int pct, int mv, bool charging, bool onUsb);
void battLogHandleSerial();   // comandos: 'd' = volcar CSV, 'r' = borrar
