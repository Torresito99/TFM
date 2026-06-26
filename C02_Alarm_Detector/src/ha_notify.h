#pragma once

// Cliente WiFi/MQTT del gateway. Publica el aviso de caida con el mismo topic y
// payload que el sistema original, de modo que la automatizacion de Home
// Assistant es indiferente al origen. La conexion no bloquea: si no hay red,
// reintenta en segundo plano y el aviso queda pendiente hasta poder enviarse.

// Inicia WiFi y configura el cliente MQTT. Llamar en setup().
void haNotifyInit();

// Mantiene la conexion y reenvia avisos pendientes. Llamar en cada loop().
void haNotifyLoop();

// Envia (o encola) el aviso de caida a Home Assistant.
void haNotifyFall();

// Publica la telemetria ambiental del BME680 en un topic independiente.
// Si no hay MQTT se omite, ya que no es una alarma.
void haPublishEnvironment(float temp, float hum, float pres, float gas,
                          float iaq, float eco2);

// Estado de los enlaces, para mostrarlo por el monitor serie.
bool haWifiConnected();
bool haMqttConnected();
