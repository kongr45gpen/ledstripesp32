#pragma once

#include <cmath>
#include <array>
#include <cstdint>
#include <optional>
#include "driver/ledc.h"
#include "config.hpp"
#include "nlohmann/json.hpp"

/**
 * TODO REMOVE THESE
 **/
enum Color_Mode
{
    Color_Temp,
    RGBWW
};

struct State
{
    bool state;
    uint8_t brightness;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t ww;
    uint8_t cw;
    uint16_t color_temperature;
    enum Color_Mode color_mode;
    float transition;
};

enum class Colour
{
    Red,
    Green,
    Blue,
    Warm_White,
    Cold_White
};

class Light
{
    std::vector<uint8_t> pins;
    std::vector<Colour> colours;

    float gamma;

    std::vector<ledc_channel_config_t> ledc_channels;

    /**
     * Applies gamma correction to a single value.
     * Input is assumed to be 8 bits, output is 12 bits.
     */
    float gamma_correction(float value)
    {
        float input = value / 255.f;
        float gamma_corrected = pow(input, gamma);
        float output = gamma_corrected * 4095.f;

        return output;
    }

    /**
     * Create the ESP32 LEDC channel configuration for a given pin.
     * @param index The index of the pin in the pins array.
     */
    ledc_channel_config_t generate_led_configuration(int index);

    bool is_modified = false;
public:
    Light() = default;

    /**
     * Create a JSON specification for this light, to be sent through MQTT to homeassistant
     */
    nlohmann::json create_homeassistant_configuration(const std::string& device_name, const nlohmann::json& device);

    /**
     * Initialise the ESP32 peripherals, including timers and channels
     */
    void initialise();

    /**
     * Render the output of the LED, as part of the FreeRTOS task loop
     */
    void render();

    inline static std::map<std::string, Light> all_lights;

    State state = {
        .state = 1,
        .brightness = 255,
        .r = 255,
        .g = 255,
        .b = 255,
        .ww = 0,
        .cw = 0,
        .color_temperature = 400,
        .color_mode = Color_Temp,
        .transition = 0.1,
    };
};