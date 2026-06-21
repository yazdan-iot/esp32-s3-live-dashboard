/**
 * @file    Config.h
 * @brief   Centralized project configuration (pins, network, timing).
 */

#pragma once

#include <cstdint>

namespace Config {

// WiFi credentials. For a real deployment, store these in NVS via a
// WiFi portal (e.g. WiFiManager) instead of hardcoding them here.
constexpr const char* WIFI_SSID     = "YOUR_WIFI_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

constexpr uint8_t  WIFI_CONNECT_RETRY_LIMIT    = 20;
constexpr uint32_t WIFI_CONNECT_RETRY_DELAY_MS = 500;

// GPIO48: onboard WS2812 LED pin on the ESP32-S3-DevKitC-1. Adjust if
// your board wires the LED elsewhere.
constexpr uint8_t  RGB_LED_PIN   = 48;
constexpr uint8_t  RGB_LED_COUNT = 1;

constexpr uint16_t HTTP_PORT = 80;

// Device is reachable at http://esp32-dashboard.local once mDNS starts.
constexpr const char* MDNS_HOSTNAME = "esp32-dashboard";

// How often metrics are recalculated and pushed to WebSocket clients.
constexpr uint32_t METRICS_BROADCAST_INTERVAL_MS = 1000;

// Sampling interval for the onboard temperature sensor.
constexpr uint32_t TEMPERATURE_SAMPLE_INTERVAL_MS = 2000;

}  // namespace Config
