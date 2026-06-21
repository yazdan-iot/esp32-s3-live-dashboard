/**
 * @file    JsonSerializer.h
 * @brief   Converts internal data structures (Snapshot, LED state) to JSON.
 */

#pragma once

#include <ArduinoJson.h>
#include "SystemMetrics.h"
#include "LedController.h"

namespace JsonSerializer {

/// Writes a full Snapshot into an existing JsonObject.
void writeMetrics(JsonObject& target, const SystemMetrics::Snapshot& snapshot);

/// Writes the current LED state (color/brightness/mode).
void writeLedState(JsonObject& target, const LedController& led);

}  // namespace JsonSerializer
