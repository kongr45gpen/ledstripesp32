#include "esp_log.h"
#include "Light.hpp"

nlohmann::json Light::create_homeassistant_configuration(const std::string& device_name, const nlohmann::json& device) {
    ESP_LOGD("LED", "Generating HomeAssistant configuration for %s", device_name.c_str());
    ESP_LOGD("LED", "Device JSON is %s", device.dump().c_str());

    if (device["type"] == "rgb") {
        if (device["pins"].size() != 3) {
            ESP_LOGE("LED", "Device %s has %d pins, but RGB devices must have 3 pins", device_name.c_str(), device["pins"].size());
            return {};
        }
        pins = device["pins"].get<std::vector<uint8_t>>();
        colours = { Colour::Red, Colour::Green, Colour::Blue };
        gamma = device.value("gamma", 4.f);
    } else {
        ESP_LOGE("LED", "Device %s has unknown type %s", device_name.c_str(), device["type"].get<std::string>().c_str());
        return {};
    }

    auto topic = "esp32/" + device_name;

    nlohmann::json j;
    j["brightness"] = true;
    j["color_mode"] = true;
    j["command_topic"] = "esp32/";
    j["device"] = {
        { "identifiers", device_name },
        { "manufacturer", "kongr45gpen" },
        { "model", CONFIG_IDF_TARGET " light, v1" },
        { "name", device_name },
        { "sw_version", "ESP32 IDF LED controller 0.2.1" },
    };
    j["json_attributes_topic"] = topic;
    j["name"] = device_name;
    j["schema"] = "json";
    j["state_topic"] = topic;
    j["supported_color_modes"] = { "rgb" }; //TODO
    j["unique_id"] = device_name + "_light_esp32";

    ESP_LOGI("LED", "Generated HomeAssistant configuration for %s: %s", device_name.c_str(), j.dump().c_str());

    ledc_channels.resize(pins.size());
    for (int i = 0; i < pins.size(); i++) {
        ledc_channels[i] = generate_led_configuration(i);
    }

    return j;
}

ledc_channel_config_t Light::generate_led_configuration(int index) {
    ledc_channel_config_t config = {
        .gpio_num = pins[index],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = static_cast<ledc_channel_t>(index),
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = { .output_invert = 0 }
    };

    return config;
}

void Light::initialise() {
    ESP_LOGI("MAIN", "Configuring LED timer...");

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,   // timer mode
        .duty_resolution = LEDC_TIMER_12_BIT, // resolution of PWM duty
        .timer_num = LEDC_TIMER_0,            // timer index
        .freq_hz = 10000,                     // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_LOGD("LED", "LED timer configured");

    // Set LED Controller with previously prepared configuration
    for (auto& channel : ledc_channels) {
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
        ESP_LOGD("LED", "LED pin %d configured", channel.gpio_num);
    }

    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    ESP_LOGD("LED", "LED fade configured");  

    ESP_LOGI("LED", "%d-pin LED initialised", pins.size());
}

void Light::render() {
    std::vector<uint16_t> duty(pins.size());

    for (int i = 0; i < pins.size(); i++) {
        if (state.state != 0) {
            if (i == 0) duty[i] = state.r * state.brightness / 255.f;
            if (i == 1) duty[i] = state.g * state.brightness / 255.f;
            if (i == 2) duty[i] = state.b * state.brightness / 255.f;
            duty[i] = static_cast<uint16_t>(gamma_correction(duty[i]));
        } else {
            duty[i] = 0;
        }
    }

    // Print avaialble heap space
    ESP_LOGD("LED", "Heap free: %d", esp_get_free_heap_size());
    ESP_LOGD("LED", "Brightness output %d %d %d [gamma = %f]", duty[0], duty[1], duty[2], gamma);

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

