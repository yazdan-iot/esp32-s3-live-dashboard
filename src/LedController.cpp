/**
 * @file    LedController.cpp
 * @brief   LED control implementation using FastLED.
 */

#include "LedController.h"
#include <FastLED.h>

// FastLED requires the LED buffer to be a static/global array (template
// constraint). Fine here since the project only ever has one instance.
static CRGB g_leds[1];

LedController::LedController(uint8_t pin, uint8_t ledCount)
    : _pin(pin), _ledCount(ledCount) {}

void LedController::begin() {
    // Pin is hardcoded here (GPIO48) because FastLED.addLeds requires a
    // template-time constant; keep it in sync with Config::RGB_LED_PIN.
    FastLED.addLeds<WS2812B, 48, GRB>(g_leds, _ledCount);
    FastLED.setBrightness(_brightness);
    renderSolid();
    FastLED.show();
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b) {
    _r = r;
    _g = g;
    _b = b;
    if (_mode == Mode::SOLID) {
        renderSolid();
        FastLED.show();
    }
}

void LedController::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

void LedController::setMode(Mode mode) {
    _mode = mode;
    if (mode == Mode::OFF) {
        FastLED.clear();
        FastLED.show();
    } else if (mode == Mode::SOLID) {
        renderSolid();
        FastLED.show();
    }
    // BREATHE/RAINBOW render on the next update() tick.
}

void LedController::update() {
    if (_mode == Mode::SOLID || _mode == Mode::OFF) {
        return;
    }

    constexpr uint32_t EFFECT_STEP_MS = 16;  // ~60fps
    const uint32_t now = millis();
    if (now - _lastEffectStepMs < EFFECT_STEP_MS) {
        return;
    }
    _lastEffectStepMs = now;

    if (_mode == Mode::BREATHE) {
        renderBreathe();
    } else if (_mode == Mode::RAINBOW) {
        renderRainbow();
    }
    FastLED.show();
}

void LedController::renderSolid() {
    g_leds[0] = CRGB(_r, _g, _b);
}

void LedController::renderBreathe() {
    // beatsin8: FastLED helper producing a 0-255 sine wave at a given BPM.
    const uint8_t breathLevel = beatsin8(15, 20, 255);
    g_leds[0] = CRGB(_r, _g, _b);
    g_leds[0].nscale8(breathLevel);
}

void LedController::renderRainbow() {
    g_leds[0] = CHSV(_rainbowHue, 255, 255);
    _rainbowHue++;  // wraps naturally at 255 -> 0
}
