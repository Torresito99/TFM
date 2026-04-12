#include "mqtt_client.h"
#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient    wifiClient;
static PubSubClient  mqtt(wifiClient);

static const char *TOPIC_FALL    = "falldetector/fall";
static const char *TOPIC_BATTERY = "falldetector/battery";

static bool wifiConnect()
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[WIFI] Conectando a %s...\n", WIFI_SSID);

    uint32_t lastLog = 0;
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - lastLog) >= 2000) {
            Serial.printf("[WIFI] Esperando conexion... status=%d\n", (int)WiFi.status());
            lastLog = millis();
        }
        delay(200);
        yield();
    }

    Serial.printf("[WIFI] Conectado: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static void wifiDisconnect()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

static bool mqttConnect()
{
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);

    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[MQTT] Conectado al broker");
        return true;
    }

    Serial.printf("[MQTT] Error conectando, rc=%d\n", mqtt.state());
    return false;
}

static constexpr uint32_t MQTT_RETRY_DELAY_MS = 2000;

static bool mqttPublishOnce(const char *topic, const char *payload, bool retained)
{
    if (!wifiConnect()) {
        return false;
    }

    if (!mqttConnect()) {
        wifiDisconnect();
        return false;
    }

    bool ok = mqtt.publish(topic, payload, retained);
    mqtt.loop();
    delay(200);
    mqtt.disconnect();
    wifiDisconnect();

    return ok;
}

static void mqttPublishWithRetry(const char *topic, const char *payload, bool retained)
{
    uint32_t attempts = 0;
    while (true) {
        attempts++;
        if (mqttPublishOnce(topic, payload, retained)) {
            Serial.printf("[MQTT] Enviado OK (%lu) -> %s\n", attempts, topic);
            return;
        }

        Serial.printf("[MQTT] Fallo envio (%lu) -> %s, reintentando en %lu ms\n",
                      attempts, topic, MQTT_RETRY_DELAY_MS);
        delay(MQTT_RETRY_DELAY_MS);
        yield();
    }
}

void mqttPublishFall()
{
    const char *payload = "{\"event\":\"fall_detected\"}";
    mqttPublishWithRetry(TOPIC_FALL, payload, true);
}

void mqttPublishBattery(uint8_t percentage, uint16_t voltage_mv, bool charging)
{
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"battery\":%u,\"voltage\":%u,\"charging\":%s}",
             percentage, voltage_mv, charging ? "true" : "false");

    mqttPublishWithRetry(TOPIC_BATTERY, payload, true);
}
