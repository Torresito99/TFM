// =====================================================================
//  ha_notify.h — Aviso a Home Assistant por MQTT.
//
//  Replica EXACTAMENTE la señal que enviaba el reloj antiguo
//  (FallDetector_Watch): topic "falldetector/fall" con payload
//  {"event":"fall_detected"} (retenido). Asi la automatizacion de
//  Home Assistant funciona igual venga del reloj o de este gateway.
//
//  WiFi/MQTT NO bloquean: si no hay red, se reintenta en segundo plano
//  y el evento de caida queda pendiente hasta poder enviarse.
// =====================================================================
#pragma once

// Inicia WiFi (no bloqueante) y configura el cliente MQTT. Llamar en setup().
void haNotifyInit();

// Mantiene WiFi/MQTT y envia lo que haya pendiente. Llamar en cada loop().
void haNotifyLoop();

// Encola/envia el aviso de caida a Home Assistant.
void haNotifyFall();

// Estado para mostrar por el monitor serie.
bool haWifiConnected();
bool haMqttConnected();
