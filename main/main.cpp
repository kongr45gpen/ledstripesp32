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
#include "cJSON.h"
#include "config.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/ledc.h"

#include "tasks.h"

const uart_port_t uart_num = UART_NUM_0;

std::optional<Configuration> config;

static const char *TAG = "Home App";

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
    // wifi_scan();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // uart_config_t uart_config = {
    //     .baud_rate = 9600,
    //     .data_bits = UART_DATA_8_BITS,
    //     .parity = UART_PARITY_DISABLE,
    //     .stop_bits = UART_STOP_BITS_1,
    //     .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
    //     .rx_flow_ctrl_thresh = 122,
    // };
    // // Configure UART parameters
    // ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // char* test_str = "This is a test string.\n";
    // uart_write_bytes(uart_num, (const char*)test_str, strlen(test_str));

    mqtt_connect();

    xTaskCreate(&led_blink,"LED_BLINK",2048,NULL,5,&led_task);
    //xTaskCreate(&nvs_manager,"NVS_MANAGER",2048,NULL,3,&nvs_task);
    // vTaskStartScheduler();

}