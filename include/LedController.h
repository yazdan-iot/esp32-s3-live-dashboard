/**
 * @file    LedController.h
 * @brief   Abstraction layer for the onboard addressable (WS2812) LED.
 */

#pragma once

#include <Arduino.h>
#include <cstdint>

class LedController {
public:
    enum class Mode : uint8_t {
        SOLID,
        BREATHE,
        RAINBOW,
        OFF
    };

    explicit LedController(uint8_t pin, uint8_t ledCount);

    /// Must be called once in setup() to initialize the FastLED driver.
    void begin();

    /// Must be called every loop() iteration to advance timed effects
    /// (Breathe/Rainbow). Internally throttled, so calling it every
    /// frame is cheap.
    void update();

    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setBrightness(uint8_t brightness);
    void setMode(Mode mode);

    uint8_t getRed()        const { return _r; }
    uint8_t getGreen()      const { return _g; }
    uint8_t getBlue()       const { return _b; }
    uint8_t getBrightness() const { return _brightness; }
    Mode    getMode()       const { return _mode; }

private:
    void renderSolid();
    void renderBreathe();
    void renderRainbow();

    const uint8_t _pin;
    const uint8_t _ledCount;

    uint8_t _r = 0, _g = 255, _b = 100;  // boot default: soft green
    uint8_t _brightness = 80;
    Mode    _mode = Mode::SOLID;

    uint32_t _lastEffectStepMs = 0;
    uint8_t  _rainbowHue       = 0;
};
