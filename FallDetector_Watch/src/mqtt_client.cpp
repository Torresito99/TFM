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
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Sin conexion");
        return false;
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

void mqttPublishFall()
{
    if (!wifiConnect()) return;

    if (mqttConnect()) {
        const char *payload = "{\"event\":\"fall_detected\"}";
        mqtt.publish(TOPIC_FALL, payload, true);
        delay(200);
        mqtt.disconnect();
        Serial.println("[MQTT] Aviso de caida enviado");
    }

    wifiDisconnect();
}

void mqttPublishBattery(uint8_t percentage, uint16_t voltage_mv, bool charging)
{
    if (!wifiConnect()) return;

    if (mqttConnect()) {
        char payload[96];
        snprintf(payload, sizeof(payload),
                 "{\"battery\":%u,\"voltage\":%u,\"charging\":%s}",
                 percentage, voltage_mv, charging ? "true" : "false");
        mqtt.publish(TOPIC_BATTERY, payload, true);
        delay(200);
        mqtt.disconnect();
        Serial.printf("[MQTT] Bateria enviada: %u%%\n", percentage);
    }

    wifiDisconnect();
}
