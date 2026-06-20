#include "ha_notify.h"
#include "config.h"
#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─── Tiempos de reintento (no bloqueante) ───────────────────────────
static constexpr uint32_t WIFI_RETRY_MS = 10000;  // reintentar WiFi cada 10 s
static constexpr uint32_t MQTT_RETRY_MS = 3000;   // reintentar MQTT cada 3 s

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static uint32_t lastWifiTry = 0;
static uint32_t lastMqttTry = 0;
static bool wifiBegun = false;
static bool pendingFall = false;   // hay una caida por enviar

static bool wifiOk() { return WiFi.status() == WL_CONNECTED; }

void haNotifyInit() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // NO esperamos aqui (no bloquea)
  wifiBegun = true;
  lastWifiTry = millis();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(256);
  mqtt.setKeepAlive(60);

  Serial.printf("[WIFI] Conectando a %s (en segundo plano)...\n", WIFI_SSID);
}

// Publica el evento de caida. Devuelve true si se envio.
static bool publishFall() {
  if (!mqtt.connected()) return false;
  bool ok = mqtt.publish(MQTT_TOPIC_FALL, MQTT_PAYLOAD_FALL, MQTT_FALL_RETAINED);
  if (ok) Serial.printf("[MQTT] CAIDA enviada -> %s %s\n",
                        MQTT_TOPIC_FALL, MQTT_PAYLOAD_FALL);
  else    Serial.println("[MQTT] Fallo al enviar la caida, queda pendiente");
  return ok;
}

static void tryWifi() {
  if (wifiOk()) return;
  uint32_t now = millis();
  if ((now - lastWifiTry) < WIFI_RETRY_MS) return;
  lastWifiTry = now;
  Serial.printf("[WIFI] Sin conexion (status=%d), reintentando...\n", (int)WiFi.status());
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void tryMqtt() {
  if (!wifiOk() || mqtt.connected()) return;
  uint32_t now = millis();
  if ((now - lastMqttTry) < MQTT_RETRY_MS) return;
  lastMqttTry = now;

  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("[MQTT] Conectado al broker (Home Assistant)");
  } else {
    Serial.printf("[MQTT] Fallo conexion broker, rc=%d\n", mqtt.state());
  }
}

// Avisa una sola vez por la IP cuando el WiFi conecta (util para diagnosticar
// que la placa esta en la misma red que el broker / Home Assistant).
static void announceWifi() {
  static bool announced = false;
  if (wifiOk() && !announced) {
    announced = true;
    Serial.printf("[WIFI] Conectado. IP de la placa: %s  (broker: %s)\n",
                  WiFi.localIP().toString().c_str(), MQTT_HOST);
  } else if (!wifiOk() && announced) {
    announced = false;
    Serial.println("[WIFI] Conexion perdida, reconectando...");
  }
}

void haNotifyLoop() {
  tryWifi();
  announceWifi();
  tryMqtt();
  mqtt.loop();

  // Si hay una caida pendiente y ya podemos enviarla, se envia.
  if (pendingFall && publishFall()) {
    pendingFall = false;
  }
}

void haNotifyFall() {
  // Intento directo para minima latencia; si no, queda pendiente.
  if (publishFall()) {
    pendingFall = false;
  } else {
    pendingFall = true;
    Serial.println("[MQTT] Caida encolada (sin conexion); se enviara al reconectar");
  }
}

bool haWifiConnected() { return wifiOk(); }
bool haMqttConnected() { return mqtt.connected(); }
