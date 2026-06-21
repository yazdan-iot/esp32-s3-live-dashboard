/**
 * @file    SystemMetrics.cpp
 * @brief   Implementation of hardware/software metrics collection.
 */

#include "SystemMetrics.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <driver/temp_sensor.h>

// Using the legacy `driver/temp_sensor.h` API instead of the newer
// `driver/temperature_sensor.h`, for compatibility with older
// arduino-esp32 core versions (incl. PlatformIO's default).

namespace SystemMetrics {

namespace {

// RTC memory survives software resets/watchdog reboots, only clearing on
// power-on reset -- used to track unexpected restarts across reboots.
RTC_DATA_ATTR uint32_t g_bootCount = 0;

uint32_t g_totalHttpRequests = 0;
uint8_t  g_connectedClients  = 0;

bool g_tempSensorStarted = false;

/// Lazily initializes the onboard temperature sensor driver.
void ensureTemperatureSensorReady() {
    if (g_tempSensorStarted) return;

    temp_sensor_config_t config = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(config);
    temp_sensor_start();
    g_tempSensorStarted = true;
}

/// Maps raw RSSI (dBm, typically -30 to -90) to a 0-100% signal strength.
uint8_t rssiToPercent(int8_t rssiDbm) {
    if (rssiDbm <= -100) return 0;
    if (rssiDbm >= -50)  return 100;
    return static_cast<uint8_t>(2 * (rssiDbm + 100));
}

/// Maps esp_reset_reason_t to a human-readable string for the UI.
String resetReasonToString(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "Power-on reset";
        case ESP_RST_EXT:       return "External reset (reset pin)";
        case ESP_RST_SW:        return "Software reset (ESP.restart)";
        case ESP_RST_PANIC:     return "Software panic / crash";
        case ESP_RST_INT_WDT:   return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "Task watchdog (likely a blocked loop)";
        case ESP_RST_WDT:       return "Other hardware watchdog";
        case ESP_RST_DEEPSLEEP: return "Woke from deep sleep";
        case ESP_RST_BROWNOUT:  return "Brownout (check power supply)";
        case ESP_RST_SDIO:      return "Reset via SDIO";
        default:                return "Unknown";
    }
}

}  // namespace

void begin() {
    g_bootCount++;
}

void incrementHttpRequestCount() {
    g_totalHttpRequests++;
}

void setConnectedClientCount(uint8_t count) {
    g_connectedClients = count;
}

Snapshot capture() {
    Snapshot s{};

    // --- Network ---
    s.wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (s.wifiConnected) {
        s.ipAddress    = WiFi.localIP().toString();
        s.macAddress   = WiFi.macAddress();
        s.ssid         = WiFi.SSID();
        s.rssiDbm      = static_cast<int8_t>(WiFi.RSSI());
        s.rssiPercent  = rssiToPercent(s.rssiDbm);
        s.gatewayIp    = WiFi.gatewayIP().toString();
        s.subnetMask   = WiFi.subnetMask().toString();
        s.dnsIp        = WiFi.dnsIP().toString();
        s.wifiChannel  = WiFi.channel();
    } else {
        s.macAddress = WiFi.macAddress();  // MAC is available even when disconnected
        s.rssiDbm = -100;
        s.rssiPercent = 0;
    }

    // --- Memory ---
    s.freeHeapBytes         = ESP.getFreeHeap();
    s.totalHeapBytes        = ESP.getHeapSize();
    s.minFreeHeapEverBytes  = ESP.getMinFreeHeap();
    s.freePsramBytes        = ESP.getFreePsram();
    s.totalPsramBytes       = ESP.getPsramSize();

    // --- Flash ---
    s.flashChipSizeBytes    = ESP.getFlashChipSize();
    s.sketchSizeBytes       = ESP.getSketchSize();
    s.freeSketchSpaceBytes  = ESP.getFreeSketchSpace();

    // --- Filesystem ---
    s.fsTotalBytes = LittleFS.totalBytes();
    s.fsUsedBytes  = LittleFS.usedBytes();

    // --- Chip ---
    s.cpuFreqMhz = ESP.getCpuFreqMHz();

    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    s.cpuCoreCount = chipInfo.cores;
    s.chipRevision = chipInfo.revision;
    s.chipModel    = "ESP32-S3";  // hardcoded; map chipInfo.model for multi-chip support
    s.sdkVersion   = ESP.getSdkVersion();

    ensureTemperatureSensorReady();
    float temperatureC = 0.0f;
    temp_sensor_read_celsius(&temperatureC);
    s.chipTemperatureCelsius = temperatureC;

    // --- Runtime ---
    // esp_timer_get_time() is in microseconds and effectively never
    // overflows, unlike millis() which wraps at ~49.7 days.
    s.uptimeMs = esp_timer_get_time() / 1000ULL;

    s.bootCount       = g_bootCount;
    s.lastResetReason = resetReasonToString(esp_reset_reason());

    // --- App-level ---
    s.totalHttpRequests          = g_totalHttpRequests;
    s.connectedWebSocketClients  = g_connectedClients;

    return s;
}

}  // namespace SystemMetrics
