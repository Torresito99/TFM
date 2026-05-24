#include "mqtt_client.h"
#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>

/* ─── Configuración ──────────────────────────────────────────────────────── */

static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000; // Reintentar reconexión cada 10s (background)
static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 3000; // Reintentar MQTT cada 3s
static constexpr uint8_t  MSG_QUEUE_SIZE = 8;

/* ─── Cola de mensajes ───────────────────────────────────────────────────── */

static const char *TOPIC_FALL    = "falldetector/fall";
static const char *TOPIC_BATTERY = "falldetector/battery";

struct MqttMessage {
    char topic[40];
    char payload[96];
    bool retained;
    bool used;
};

static MqttMessage msgQueue[MSG_QUEUE_SIZE];
static uint8_t queueHead = 0;

static void queuePush(const char *topic, const char *payload, bool retained)
{
    MqttMessage &msg = msgQueue[queueHead];
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';
    msg.retained = retained;
    msg.used = true;
    queueHead = (queueHead + 1) % MSG_QUEUE_SIZE;
}

/* ─── WiFi & MQTT ────────────────────────────────────────────────────────── */

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static uint32_t lastWifiReconnectAttempt = 0;
static uint32_t lastMqttReconnectAttempt = 0;
static bool wifiWasConnected = false;

static bool wifiIsConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

static bool mqttIsConnected()
{
    return mqtt.connected();
}

/* ─── Inicialización ─────────────────────────────────────────────────────── */

void mqttInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    // Conexión inicial bloqueante — esperar hasta conectar
    // Al inicio no hay detección de caídas, así que podemos esperar
    Serial.printf("[WIFI] Conectando a %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Esperar pacientemente sin interrumpir el proceso de conexión
    uint32_t lastLog = millis();
    while (!wifiIsConnected()) {
        delay(500);
        if ((millis() - lastLog) >= 5000) {
            Serial.printf("[WIFI] Esperando... status=%d\n", (int)WiFi.status());
            lastLog = millis();
        }
    }

    Serial.printf("[WIFI] Conectado: %s\n", WiFi.localIP().toString().c_str());
    wifiWasConnected = true;

    // Activar modem sleep para ahorrar energía manteniendo conexión
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    Serial.println("[WIFI] Modem sleep activado");

    // Esperar a que la pila de red esté lista
    delay(1000);

    // Conectar MQTT ahora que hay WiFi
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(60);

    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[MQTT] Conectado al broker");
    } else {
        Serial.printf("[MQTT] Fallo inicial, rc=%d — reintentara en background\n", mqtt.state());
    }
}

/* ─── Loop no-bloqueante ─────────────────────────────────────────────────── */

static void tryWifiReconnect()
{
    uint32_t now = millis();
    if ((now - lastWifiReconnectAttempt) < WIFI_RECONNECT_INTERVAL_MS) {
        return;
    }
    lastWifiReconnectAttempt = now;

    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasConnected) {
            Serial.printf("[WIFI] Reconectado: %s\n", WiFi.localIP().toString().c_str());
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
            wifiWasConnected = true;
        }
        return;
    }

    // Forzar reintento activo — setAutoReconnect no siempre funciona
    wifiWasConnected = false;
    Serial.printf("[WIFI] Desconectado (status=%d), forzando reconexion...\n", (int)WiFi.status());
    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void tryMqttReconnect()
{
    if (!wifiIsConnected() || mqttIsConnected()) {
        return;
    }

    uint32_t now = millis();
    if ((now - lastMqttReconnectAttempt) < MQTT_RECONNECT_INTERVAL_MS) {
        return;
    }
    lastMqttReconnectAttempt = now;

    // connect() es rápido si el broker responde — no bloquea más de ~1-2s
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[MQTT] Conectado al broker");
    } else {
        Serial.printf("[MQTT] Fallo conexion, rc=%d\n", mqtt.state());
    }
}

static void processQueue()
{
    if (!mqttIsConnected()) {
        return;
    }

    for (uint8_t i = 0; i < MSG_QUEUE_SIZE; i++) {
        MqttMessage &msg = msgQueue[i];
        if (!msg.used) {
            continue;
        }

        if (mqtt.publish(msg.topic, msg.payload, msg.retained)) {
            Serial.printf("[MQTT] Enviado -> %s\n", msg.topic);
            msg.used = false;
        } else {
            // Si falla un envío, parar — probablemente perdimos conexión
            Serial.printf("[MQTT] Fallo envio -> %s\n", msg.topic);
            break;
        }
    }
}

void mqttLoop()
{
    tryWifiReconnect();
    tryMqttReconnect();
    mqtt.loop();  // Mantener keepalive MQTT
    processQueue();
}

/* ─── API pública (no bloqueante) ────────────────────────────────────────── */

void mqttPublishFall()
{
    const char *payload = "{\"event\":\"fall_detected\"}";

    // Intento directo si ya estamos conectados (mínima latencia para caídas)
    if (mqttIsConnected()) {
        if (mqtt.publish(TOPIC_FALL, payload, true)) {
            Serial.printf("[MQTT] Caida enviada directamente\n");
            return;
        }
    }

    // Si no se pudo, encolar
    Serial.println("[MQTT] Caida encolada para envio posterior");
    queuePush(TOPIC_FALL, payload, true);
}

void mqttPublishBattery(uint8_t percentage, uint16_t voltage_mv, bool charging)
{
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"battery\":%u,\"voltage\":%u,\"charging\":%s}",
             percentage, voltage_mv, charging ? "true" : "false");

    if (mqttIsConnected()) {
        if (mqtt.publish(TOPIC_BATTERY, payload, true)) {
            Serial.printf("[MQTT] Bateria enviada directamente\n");
            return;
        }
    }

    queuePush(TOPIC_BATTERY, payload, true);
}
