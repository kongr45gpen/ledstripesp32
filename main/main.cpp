#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "SSD1306.hpp"
#include "config.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"
#include "Light.hpp"
#include "main.hpp"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/ledc.h"
#include "tasks.h"

const uart_port_t uart_num = UART_NUM_0;

std::optional<Configuration> config;
std::optional<SSD1306IDF> display;

TaskHandle_t led_task = NULL;
TaskHandle_t nvs_task = NULL;

void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
        ESP_LOGE(TAG, "Error description: %s", esp_err_to_name(error_code));
    }
}

void nvs_manager(void *pvParams) {
    // // Initialise state from storage
    // nvs_handle_t handle;
    // esp_err_t err;

    // err = nvs_open("storage", NVS_READWRITE, &handle);
    // log_error_if_nonzero("from NVS storage open", err);

    // // Initialise config from memory
    // {
    //     float transition;
    //     err = nvs_get_u32(handle, "transition", (uint32_t*)(&transition));
    //     log_error_if_nonzero("reading NVS transition", err);

    //     if (err == ESP_OK) {
    //         ESP_LOGI("NVS", "Read transition from storage: %f s", transition);
    //         state.transition = transition;
    //     }
    // }

    // while (true) {
    //     xTaskNotifyWait(0, 0, 0, portMAX_DELAY);
    //     ESP_LOGI("NVS", "Storing current configuration to NVS");

    //     err = nvs_set_u32(handle, "transition", *((uint32_t*)(&(state.transition))));
    //     log_error_if_nonzero("writing NVS transition", err);

    //     err = nvs_commit(handle);
    //     log_error_if_nonzero("from NVS commit", err);
    // }

    // nvs_close(handle); // never called
}

extern "C" void app_main() {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("LED", ESP_LOG_DEBUG);

    //Initialize NVS
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    config = read_config();

    if (config->has_display()) {
        ESP_LOGI(TAG, "Initialising display");
        i2c_config_t i2c_conf = config->get_display_i2c_config();
        auto i2c_port = static_cast<i2c_port_t>(0);
        i2c_param_config(i2c_port, &i2c_conf);
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(i2c_port, I2C_MODE_MASTER, false, false, 0));

        display.emplace(i2c_port);
        display->init();
        display->drawString(0, 0, "Connecting to WiFi...");
        display->drawString(0, 12, config->ssid().data());
        display->display();
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    if (display) {
        display->clear();
        display->drawString(0, 0, "Connecting to MQTT...");
        display->drawString(0, 12, config->mqtt_host().c_str());
    }

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,   // timer mode
        .duty_resolution = LEDC_TIMER_12_BIT, // resolution of PWM duty
        .timer_num = LEDC_TIMER_0,            // timer index
        .freq_hz = 10000,                     // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_LOGI("LED", "LED timer configured");

    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    ESP_LOGD("LED", "LED fade configured");  

    for (auto& light : config->lights().items()) {
        Light& light_object = Light::all_lights[light.key()];

        light_object.create_homeassistant_configuration(light.key(), light.value());
    }

    mqtt_connect();

    if (display) {
        display->clear();
        display->drawString(0, 0, "Connected!");
        display->drawString(0, 12, "Hostname:");
        display->drawString(50, 12, config->hostname().c_str());
        // For every light print info
        int i = 0;
        for (auto& light : Light::all_lights) {
            static char buf[30];
            display->drawStringf(0, 24 + 12 * i, buf, "Light %d: %s", i, light.first.c_str());
            i++;
        }
        display->display();
    }

    xTaskCreate(&led_blink,"LED_BLINK",4096,NULL,5,&led_task);

    if (display) {
        xTaskCreate(&display_update,"DISPLAY",4096,NULL,1,NULL);
    }
    //xTaskCreate(&nvs_manager,"NVS_MANAGER",2048,NULL,3,&nvs_task);
    // vTaskStartScheduler();

}