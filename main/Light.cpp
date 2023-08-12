#include "esp_log.h"
#include "main.hpp"
#include "Light.hpp"

void Light::create_homeassistant_configuration(const std::string& device_name, const nlohmann::json& device) {
    ESP_LOGD("LED", "Generating HomeAssistant configuration for %s", device_name.c_str());
    ESP_LOGD("LED", "Device JSON is %s", device.dump().c_str());

    std::string homeassistant_color_mode;

    if (device["type"] == "rgb") {
        if (device["pins"].size() != 3) {
            ESP_LOGE("LED", "Device %s has %d pins, but RGB devices must have 3 pins", device_name.c_str(), device["pins"].size());
            return;
        }
        pins = device["pins"].get<std::vector<uint8_t>>();
        colours = { Colour::Red, Colour::Green, Colour::Blue };
        gamma = device.value("gamma", 4.f);

        homeassistant_color_mode = "rgb";
        state.color_mode = Color_Mode::RGB;
    } else if (device["type"] == "dimmer") {
        pins = { device["pin"] };
        colours = { Colour::Warm_White };
        gamma = device.value("gamma", 4.f);

        homeassistant_color_mode = "brightness";
        state.color_mode = Color_Mode::Brightness;
    } else {
        ESP_LOGE("LED", "Device %s has unknown type %s", device_name.c_str(), device["type"].get<std::string>().c_str());
        return;
    }

    auto topic = "esp32/" + device_name;

    hass_configuration["brightness"] = true;
    hass_configuration["color_mode"] = true;
    hass_configuration["command_topic"] = topic + "/set";
    hass_configuration["device"] = {
        { "identifiers", config->unique_id() },
        { "manufacturer", "kongr45gpen" },
        { "model", CONFIG_IDF_TARGET " light, v1" },
        { "name", config->unique_id() },
        { "sw_version", "ESP32 IDF LED controller 0.2.1" },
    };
    hass_configuration["json_attributes_topic"] = topic;
    hass_configuration["name"] = device_name;
    hass_configuration["schema"] = "json";
    hass_configuration["state_topic"] = topic;
    hass_configuration["supported_color_modes"] = { homeassistant_color_mode };
    hass_configuration["unique_id"] = device_name + "_light_esp32";

    ESP_LOGI("LED", "Read configuration for %s", device_name.c_str());

    ledc_channels.resize(pins.size());
    for (int i = 0; i < pins.size(); i++) {
        ledc_channels[i] = generate_led_configuration(i);
    }
}

const nlohmann::json& Light::get_homeassistant_configuration() const {
    return hass_configuration;
}

ledc_channel_config_t Light::generate_led_configuration(int index) {
    ledc_channel_config_t config = {
        .gpio_num = pins[index],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = static_cast<ledc_channel_t>(current_pwm_channel++),
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = { .output_invert = 0 }
    };

    return config;
}

void Light::initialise() {
    // Set LED Controller with previously prepared configuration
    for (auto& channel : ledc_channels) {
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
        ESP_LOGD("LED", "LED pin %d configured", channel.gpio_num);
    }

    ESP_LOGI("LED", "%d-pin LED initialised", pins.size());
}

void Light::render() {
    std::vector<uint16_t> duty(pins.size());

    for (int i = 0; i < pins.size(); i++) {
        if (state.state != 0) {
            if (pins.size() == 3) {
                if (i == 0) duty[i] = state.r * state.brightness / 255.f;
                if (i == 1) duty[i] = state.g * state.brightness / 255.f;
                if (i == 2) duty[i] = state.b * state.brightness / 255.f;
            } else {
                duty[i] = state.brightness;
            }
            duty[i] = static_cast<uint16_t>(gamma_correction(duty[i]));
        } else {
            duty[i] = 0;
        }
    }

    // Print avaialble heap space
    ESP_LOGD("LED", "Heap free: %d", esp_get_free_heap_size());
    std::string duty_string = "Brightness output: ";
    for (auto d : duty) {
        duty_string += std::to_string(d) + " ";
    }
    duty_string += "[gamma = " + std::to_string(gamma) + "]";
    ESP_LOGD("LED", "%s", duty_string.c_str());

    if (state.transition != 0) {
        // Sanity checks
        if (state.transition > 3) state.transition = 3;
        if (state.transition < 0) state.transition = 0;

        ESP_LOGD("LED", "Starting transition [%f s]", state.transition);

        for (int i = 0; i < pins.size(); i++) {
            ledc_set_fade_with_time(
                ledc_channels[i].speed_mode,
                ledc_channels[i].channel,
                duty[i],
                1000 * state.transition);
            ledc_fade_start(
                ledc_channels[i].speed_mode, 
                ledc_channels[i].channel, 
                i == (pins.size() - 1) ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT);
        }

        ESP_LOGD("LED", "Transition complete");
    } else {
        for (int i = 0; i < 3; i++) {
            ledc_set_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel, duty[i]);
            ledc_update_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel);
        }
    }
}

