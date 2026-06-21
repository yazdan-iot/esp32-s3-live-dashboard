/**
 * @file    SystemMetrics.h
 * @brief   Collects hardware/software status from the ESP32-S3.
 */

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace SystemMetrics {

/// A consistent snapshot of device state at one point in time.
struct Snapshot {
    // --- Network ---
    bool     wifiConnected;
    String   ipAddress;
    String   macAddress;
    String   ssid;
    int8_t   rssiDbm;
    uint8_t  rssiPercent;
    String   gatewayIp;
    String   subnetMask;
    String   dnsIp;
    uint8_t  wifiChannel;

    // --- Memory ---
    uint32_t freeHeapBytes;
    uint32_t totalHeapBytes;
    uint32_t minFreeHeapEverBytes;
    uint32_t freePsramBytes;
    uint32_t totalPsramBytes;
    uint32_t flashChipSizeBytes;
    uint32_t sketchSizeBytes;
    uint32_t freeSketchSpaceBytes;

    // --- Filesystem (LittleFS) ---
    uint32_t fsTotalBytes;
    uint32_t fsUsedBytes;

    // --- Chip ---
    uint32_t cpuFreqMhz;
    uint8_t  cpuCoreCount;
    float    chipTemperatureCelsius;
    String   chipModel;
    uint8_t  chipRevision;
    String   sdkVersion;

    // --- Runtime ---
    uint64_t uptimeMs;
    uint32_t bootCount;
    String   lastResetReason;

    // --- App-level ---
    uint32_t totalHttpRequests;
    uint8_t  connectedWebSocketClients;
};

/// Call once in setup().
void begin();

/// Returns a fresh snapshot of all current metrics.
Snapshot capture();

void incrementHttpRequestCount();
void setConnectedClientCount(uint8_t count);

}  // namespace SystemMetrics
