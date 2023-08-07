#include "esp_log.h"
#include "Light.hpp"

template <int Pin_Count>
ledc_channel_config_t Light<Pin_Count>::generate_led_configuration(int index) {
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

template <int Pin_Count>
void Light<Pin_Count>::initialise() {
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

    ESP_LOGI("LED", "%d-pin LED initialised", Pin_Count);
}

template <int Pin_Count>
void Light<Pin_Count>::render(State state) {
    uint16_t duty[Pin_Count] = { 0 };

    for (int i = 0; i < Pin_Count; i++) {
        if (state.state != 0) {
            if (i == 0) duty[i] = state.r * state.brightness / 255.f;
            if (i == 1) duty[i] = state.g * state.brightness / 255.f;
            if (i == 2) duty[i] = state.b * state.brightness / 255.f;
            duty[i] = static_cast<uint16_t>(gamma_correction(duty[i]));
        } else {
            duty[i] = 0;
        }
    }

    ESP_LOGD("LED", "Brightness output %d %d %d [gamma = %f]", duty[0], duty[1], duty[2], gamma);

    if (state.transition != 0) {
        // Sanity checks
        if (state.transition > 3) state.transition = 3;
        if (state.transition < 0) state.transition = 0;

        ESP_LOGD("LED", "Starting transition [%f s]", state.transition);

        for (int i = 0; i < Pin_Count; i++) {
            ledc_set_fade_with_time(
                ledc_channels[i].speed_mode,
                ledc_channels[i].channel,
                duty[i],
                1000 * state.transition);
            ledc_fade_start(
                ledc_channels[i].speed_mode, 
                ledc_channels[i].channel, 
                i == (Pin_Count - 1) ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT);
        }

        ESP_LOGD("LED", "Transition complete");
    } else {
        for (int i = 0; i < 3; i++) {
            ledc_set_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel, duty[i]);
            ledc_update_duty(ledc_channels[i].speed_mode, ledc_channels[i].channel);
        }
    }
}

template class Light<3>;