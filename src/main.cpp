/**
 * @file    main.cpp
 * @brief   Entry point: sets up WiFi, LittleFS, the async web server, and
 *          a WebSocket for streaming live system metrics to the dashboard.
 *
 * Frontend files (HTML/CSS/JS) are served from LittleFS rather than
 * embedded in flash via PROGMEM, so the UI can be updated independently
 * of the firmware. ESPAsyncWebServer is used instead of a synchronous
 * server so multiple clients can connect concurrently and metrics can be
 * pushed over WebSocket instead of relying on client-side polling.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

#include "Config.h"
#include "LedController.h"
#include "SystemMetrics.h"
#include "JsonSerializer.h"

AsyncWebServer  g_server(Config::HTTP_PORT);
AsyncWebSocket  g_webSocket("/ws");
LedController   g_led(Config::RGB_LED_PIN, Config::RGB_LED_COUNT);

uint32_t g_lastMetricsBroadcastMs = 0;

// ----------------------------------------------------------------------------
// WiFi
// ----------------------------------------------------------------------------

/// Connects to WiFi with simple retry; restarts the device if it can't
/// connect within Config::WIFI_CONNECT_RETRY_LIMIT attempts.
void connectToWiFi() {
    Serial.printf("[WiFi] Connecting to: %s\n", Config::WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < Config::WIFI_CONNECT_RETRY_LIMIT) {
        delay(Config::WIFI_CONNECT_RETRY_DELAY_MS);
        Serial.print('.');
        retries++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] Connection failed -- restarting in 3s.");
        delay(3000);
        ESP.restart();
    }

    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ----------------------------------------------------------------------------
// WebSocket
// ----------------------------------------------------------------------------

/// Handles incoming WebSocket messages, e.g.:
///   {"action":"setColor","r":255,"g":0,"b":0}
///   {"action":"setBrightness","value":128}
///   {"action":"setMode","mode":"rainbow"}
void handleWebSocketMessage(AsyncWebSocketClient* client, const uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* action = doc["action"];
    if (action == nullptr) return;

    if (strcmp(action, "setColor") == 0) {
        const uint8_t r = doc["r"] | 0;
        const uint8_t g = doc["g"] | 0;
        const uint8_t b = doc["b"] | 0;
        g_led.setColor(r, g, b);

    } else if (strcmp(action, "setBrightness") == 0) {
        const uint8_t value = doc["value"] | 80;
        g_led.setBrightness(value);

    } else if (strcmp(action, "setMode") == 0) {
        const char* modeStr = doc["mode"] | "solid";
        if (strcmp(modeStr, "breathe") == 0)      g_led.setMode(LedController::Mode::BREATHE);
        else if (strcmp(modeStr, "rainbow") == 0) g_led.setMode(LedController::Mode::RAINBOW);
        else if (strcmp(modeStr, "off") == 0)     g_led.setMode(LedController::Mode::OFF);
        else                                       g_led.setMode(LedController::Mode::SOLID);
    }

    // Broadcast the new LED state to all clients so every connected
    // dashboard stays in sync.
    JsonDocument outDoc;
    JsonObject ledState = outDoc["led"].to<JsonObject>();
    JsonSerializer::writeLedState(ledState, g_led);

    String payload;
    serializeJson(outDoc, payload);
    g_webSocket.textAll(payload);
}

void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                          client->remoteIP().toString().c_str());
            SystemMetrics::setConnectedClientCount(server->count());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            SystemMetrics::setConnectedClientCount(server->count());
            break;

        case WS_EVT_DATA:
            handleWebSocketMessage(client, data, len);
            break;

        default:
            break;
    }
}

// ----------------------------------------------------------------------------
// HTTP routes
// ----------------------------------------------------------------------------

/// Registers HTTP routes: a status snapshot endpoint (for instant data on
/// page load, before the first WebSocket message arrives) plus static
/// frontend files served from LittleFS.
void setupHttpRoutes() {
    g_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        SystemMetrics::incrementHttpRequestCount();

        JsonDocument doc;
        const SystemMetrics::Snapshot snapshot = SystemMetrics::capture();

        JsonObject metricsObj = doc["metrics"].to<JsonObject>();
        JsonSerializer::writeMetrics(metricsObj, snapshot);

        JsonObject ledObj = doc["led"].to<JsonObject>();
        JsonSerializer::writeLedState(ledObj, g_led);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    g_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    g_server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "404: Not found");
    });
}

// ----------------------------------------------------------------------------
// setup() / loop()
// ----------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== ESP32-S3 Dashboard -- starting up ===");

    SystemMetrics::begin();
    g_led.begin();

    if (!LittleFS.begin(true)) {  // true = auto-format if unformatted
        Serial.println("[FS] LittleFS init failed! Web files will be unavailable.");
    }

    connectToWiFi();

    if (MDNS.begin(Config::MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", Config::HTTP_PORT);
        Serial.printf("[mDNS] Active at http://%s.local\n", Config::MDNS_HOSTNAME);
    }

    g_webSocket.onEvent(onWebSocketEvent);
    g_server.addHandler(&g_webSocket);

    setupHttpRoutes();
    g_server.begin();

    Serial.println("=== Setup complete -- dashboard is live ===\n");
}

void loop() {
    g_led.update();
    g_webSocket.cleanupClients();  // drops dead clients (e.g. closed tabs)

    const uint32_t now = millis();
    if (now - g_lastMetricsBroadcastMs >= Config::METRICS_BROADCAST_INTERVAL_MS) {
        g_lastMetricsBroadcastMs = now;

        if (g_webSocket.count() > 0) {
            JsonDocument doc;
            const SystemMetrics::Snapshot snapshot = SystemMetrics::capture();
            JsonObject metricsObj = doc["metrics"].to<JsonObject>();
            JsonSerializer::writeMetrics(metricsObj, snapshot);

            String payload;
            serializeJson(doc, payload);
            g_webSocket.textAll(payload);
        }
    }
}
