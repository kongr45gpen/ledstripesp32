#include "esp_log.h"
#include "main.hpp"
#include "Light.hpp"
#include "led_strip.h"

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

        homeassistant_color_mode = "rgb";
        state.color_mode = Color_Mode::RGB;
    } else if (device["type"] == "rgbww") {
        if (device["pins"].size() != 5) {
            ESP_LOGE("LED", "Device %s has %d pins, but RGBWWCW devices must have 5 pins", device_name.c_str(), device["pins"].size());
            return;
        }
        pins = device["pins"].get<std::vector<uint8_t>>();
        colours = { Colour::Red, Colour::Green, Colour::Blue, Colour::Warm_White, Colour::Cold_White };

        homeassistant_color_mode = "rgbww";
        state.color_mode = Color_Mode::RGBWWCW;
    } else if (device["type"] == "dimmer") {
        pins = { device["pin"] };
        colours = { Colour::Warm_White };

        homeassistant_color_mode = "brightness";
        state.color_mode = Color_Mode::Brightness;
    } else if (device["type"] == "addressable") {
        pins = { device["pin"], device["pin"], device["pin"] };
        
        is_addressable = true;

        homeassistant_color_mode = "rgb";
        state.color_mode = Color_Mode::RGB;

        if (device.contains("size")) {
            if (device.contains("width")) {
                addressable_led_width = device["width"];
                addressable_led_height = device["size"].get<uint16_t>() / addressable_led_width;
            } else if (device.contains("height")) {
                addressable_led_height = device["height"];
                addressable_led_width = device["size"].get<uint16_t>() / addressable_led_height;
            } else {
                addressable_led_width = device["size"];
                addressable_led_height = 1;
            }
        } else if (device.contains("width") && device.contains("height")) {
            addressable_led_width = device["width"];
            addressable_led_height = device["height"];
        } else {
            ESP_LOGE("LED", "Device %s is addressable, but no size is specified", device_name.c_str());
            addressable_led_height = 1;
            addressable_led_width = 1;
            return;
        }
    } else {
        ESP_LOGE("LED", "Device %s has unknown type %s", device_name.c_str(), device["type"].get<std::string>().c_str());
        return;
    }

    if (pins.size() > current_brightness.max_size()) {
        ESP_LOGE("LED", "Device %s has %d pins, but max %d are supported", device_name.c_str(), pins.size(), current_brightness.max_size());
        return;
    }

    gamma = device.value("gamma", 4.f);

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

    if (!is_addressable) {
        ledc_channels.resize(pins.size());
        for (int i = 0; i < pins.size(); i++) {
            ledc_channels[i] = generate_led_configuration(i);
        }
    } else {
        addressable_configuration.emplace();
        memset(&*addressable_configuration, 0, sizeof(*addressable_configuration));
        addressable_configuration->rgb_led_type = RGB_LED_TYPE_WS2812;
        addressable_configuration->rmt_channel = RMT_CHANNEL_1;
        addressable_configuration->rmt_interrupt_num = 19;
        addressable_configuration->gpio = static_cast<gpio_num_t>(pins[0]);
        addressable_configuration->led_strip_buf_1 = (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3);
        addressable_configuration->led_strip_buf_2 = (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3);
        addressable_configuration->led_strip_length = addressable_led_width * addressable_led_height;
        addressable_configuration->access_semaphore = xSemaphoreCreateBinary();
        // led_strip_t cfg = {
        //     .rgb_led_type = RGB_LED_TYPE_WS2812,
        //     .led_strip_length = static_cast<uint32_t>(addressable_led_width * addressable_led_height),
        //     .rmt_channel = RMT_CHANNEL_1,
        //     .rmt_interrupt_num = 19,
        //     .gpio = static_cast<gpio_num_t>(pins[0]),
        //     .showing_buf_1 = false,
        //     .led_strip_buf_1 = (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3),
        //     .led_strip_buf_2 = (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3),
        //     .access_semaphore = xSemaphoreCreateBinary()
        // };

        // //TODO: This is terrible, rewrite it in a better way
        // ESP_LOGE("LED", "Addressable LED strip configuration. SIZEOF(OPTIONAL) == %d", sizeof(std::optional<led_strip_t>));
        // auto opt = std::make_optional<led_strip_t>(cfg);
        // memcpy((void*)&addressable_configuration, (void*)&opt, sizeof(opt));

        // addressable_configuration.emplace({
        //     static_cast<uint32_t>(RGB_LED_TYPE_WS2812),
        //     static_cast<uint32_t>(addressable_led_width * addressable_led_height),
        //     static_cast<uint32_t>(RMT_CHANNEL_1),
        //     uint32_t{19},
        //     static_cast<uint32_t>(pins[0]),
        //     false,
        //     (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3),
        //     (led_color_t*)malloc(sizeof(uint32_t) * addressable_led_width * addressable_led_height * 3),
        //     xSemaphoreCreateBinary()
        // });

        ESP_ERROR_CHECK(!led_strip_init(&(*addressable_configuration)));
        ESP_LOGI("LED", "Addressable LED strip initialised at pin %d", pins[0]);
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
            if (pins.size() >= 3) {
                if (i == 0) duty[i] = state.r * state.brightness / 255.f;
                if (i == 1) duty[i] = state.g * state.brightness / 255.f;
                if (i == 2) duty[i] = state.b * state.brightness / 255.f;
                if (i == 3) duty[i] = state.ww * state.brightness / 255.f;
                if (i == 4) duty[i] = state.cw * state.brightness / 255.f;
            } else {
                duty[i] = state.brightness;
            }
            
            if (i < current_brightness.size()) [[likely]] {
                current_brightness[i] = duty[i];
            }
            ESP_LOGD("LED", "Raw brt [%d] = %.1f", i, duty[i]);

            duty[i] = static_cast<uint16_t>(gamma_correction(duty[i]));
        } else {
            duty[i] = 0;

            if (i < current_brightness.size()) [[likely]] {
                current_brightness[i] = duty[i];
            }
        }
    }

    ESP_LOGD("LED", "Heap free: %d", esp_get_free_heap_size());
    std::string duty_string = "Brightness output: ";
    for (auto d : duty) {
        duty_string += std::to_string(d) + " ";
    }
    duty_string += "[gamma = " + std::to_string(gamma) + "]";
    ESP_LOGI("LED", "%s", duty_string.c_str());

    if (is_addressable) {
        uint16_t number_of_leds_on_0 = ceilf((duty[0] / 4095.f) * addressable_led_width * addressable_led_height);
        uint16_t number_of_leds_on_1 = ceilf((duty[1] / 4095.f) * addressable_led_width * addressable_led_height);
        uint16_t number_of_leds_on_2 = ceilf((duty[2] / 4095.f) * addressable_led_width * addressable_led_height);
        uint8_t actual_brightness_0 = roundf(((float)number_of_leds_on_0 / (addressable_led_width * addressable_led_height)) * 255);
        uint8_t actual_brightness_1 = roundf(((float)number_of_leds_on_1 / (addressable_led_width * addressable_led_height)) * 255);
        uint8_t actual_brightness_2 = roundf(((float)number_of_leds_on_2 / (addressable_led_width * addressable_led_height)) * 255);

        ESP_LOGD("LED", "Rendering addressable LED strip");
        for (int i = 0; i < addressable_led_width * addressable_led_height; i++) {
            led_strip_set_pixel_rgb(&*addressable_configuration, i, 
                (i < number_of_leds_on_0) ? actual_brightness_0 : 0,
                (i < number_of_leds_on_1) ? actual_brightness_1 : 0,
                (i < number_of_leds_on_2) ? actual_brightness_2 : 0
            );
        }
        ESP_ERROR_CHECK_WITHOUT_ABORT(!led_strip_show(&*addressable_configuration));
        return;
    }

    if (state.transition > 0) {
        // Sanity checks
        if (state.transition > 3) state.transition = 3;

        ESP_LOGD("LED", "Starting transition [%f s]", state.transition);

        // Workaround issue with transitioning between maximum duty cycles
#if defined(CONFIG_IDF_TARGET_ESP32) 
        for (int i = 0; i < pins.size(); i++) {
            auto current_duty = ledc_get_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel);
            ESP_LOGD("LED", "[%d] Current duty cycle for pin %d is %ld", i, pins[i], current_duty);
            constexpr uint32_t maximum_duty = (1 << 12) - 1;
            if (duty[i] == maximum_duty && current_duty == maximum_duty) {
                // We don't continue so that the transition wait is not messed up
                ledc_set_duty_and_update(ledc_channels[i].speed_mode, ledc_channels[i].channel, maximum_duty - 1, 0);
                // Duty cycle is only updated after a PWM cycle
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
#endif

        for (int i = 0; i < pins.size(); i++) {
            ledc_set_fade_time_and_start(
                ledc_channels[i].speed_mode,
                ledc_channels[i].channel,
                duty[i],
                1000 * state.transition,
                i == (pins.size() - 1) ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT
            );
        }

        ESP_LOGD("LED", "Transition complete");
    } else {
        for (int i = 0; i < 3; i++) {
            ledc_set_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel, duty[i]);
            ledc_update_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel);
        }
    }
}

