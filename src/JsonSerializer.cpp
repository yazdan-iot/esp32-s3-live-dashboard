/**
 * @file    JsonSerializer.cpp
 * @brief   Implementation of internal-struct-to-JSON conversion.
 */

#include "JsonSerializer.h"

namespace JsonSerializer {

namespace {
const char* ledModeToString(LedController::Mode mode) {
    switch (mode) {
        case LedController::Mode::SOLID:   return "solid";
        case LedController::Mode::BREATHE: return "breathe";
        case LedController::Mode::RAINBOW: return "rainbow";
        case LedController::Mode::OFF:     return "off";
        default:                           return "solid";
    }
}
}  // namespace

void writeMetrics(JsonObject& target, const SystemMetrics::Snapshot& s) {
    JsonObject network = target["network"].to<JsonObject>();
    network["connected"]  = s.wifiConnected;
    network["ip"]          = s.ipAddress;
    network["mac"]         = s.macAddress;
    network["ssid"]        = s.ssid;
    network["rssiDbm"]     = s.rssiDbm;
    network["rssiPercent"] = s.rssiPercent;
    network["gateway"]     = s.gatewayIp;
    network["subnet"]      = s.subnetMask;
    network["dns"]         = s.dnsIp;
    network["channel"]     = s.wifiChannel;

    JsonObject memory = target["memory"].to<JsonObject>();
    memory["freeHeap"]    = s.freeHeapBytes;
    memory["totalHeap"]   = s.totalHeapBytes;
    memory["minFreeHeap"] = s.minFreeHeapEverBytes;
    memory["freePsram"]   = s.freePsramBytes;
    memory["totalPsram"]  = s.totalPsramBytes;

    JsonObject storage = target["storage"].to<JsonObject>();
    storage["flashSize"]       = s.flashChipSizeBytes;
    storage["sketchSize"]      = s.sketchSizeBytes;
    storage["freeSketchSpace"] = s.freeSketchSpaceBytes;
    storage["fsTotal"]         = s.fsTotalBytes;
    storage["fsUsed"]          = s.fsUsedBytes;

    JsonObject chip = target["chip"].to<JsonObject>();
    chip["model"]        = s.chipModel;
    chip["revision"]     = s.chipRevision;
    chip["cpuFreqMhz"]   = s.cpuFreqMhz;
    chip["coreCount"]    = s.cpuCoreCount;
    chip["temperatureC"] = serialized(String(s.chipTemperatureCelsius, 1));
    chip["sdkVersion"]   = s.sdkVersion;

    JsonObject runtime = target["runtime"].to<JsonObject>();
    runtime["uptimeMs"]        = s.uptimeMs;
    runtime["bootCount"]       = s.bootCount;
    runtime["lastResetReason"] = s.lastResetReason;
    runtime["httpRequests"]    = s.totalHttpRequests;
    runtime["wsClients"]       = s.connectedWebSocketClients;
}

void writeLedState(JsonObject& target, const LedController& led) {
    target["mode"]       = ledModeToString(led.getMode());
    target["r"]          = led.getRed();
    target["g"]          = led.getGreen();
    target["b"]          = led.getBlue();
    target["brightness"] = led.getBrightness();
}

}  // namespace JsonSerializer
