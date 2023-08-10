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
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "config.h"
#include "config.hpp"
#include "wifi.hpp"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/ledc.h"

#include "tasks.h"

const uart_port_t uart_num = UART_NUM_0;

std::optional<Configuration> config;

enum Color_Mode {
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

struct State state = {
    .state = 1,
    .brightness = 100,
    .r = 255,
    .g = 255,
    .b = 255,
    .ww = 0,
    .cw = 0,
    .color_temperature = 400,
    .color_mode = Color_Temp,
    .transition = 0.1,
};

static const char *TAG = "Home App";

TaskHandle_t led_task = NULL;
TaskHandle_t nvs_task = NULL;

static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
        ESP_LOGE(TAG, "Error description: %s", esp_err_to_name(error_code));
    }
}

void nvs_manager(void *pvParams) {
    // Initialise state from storage
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &handle);
    log_error_if_nonzero("from NVS storage open", err);

    // Initialise config from memory
    {
        float transition;
        err = nvs_get_u32(handle, "transition", (uint32_t*)(&transition));
        log_error_if_nonzero("reading NVS transition", err);

        if (err == ESP_OK) {
            ESP_LOGI("NVS", "Read transition from storage: %f s", transition);
            state.transition = transition;
        }
    }

    while (true) {
        xTaskNotifyWait(0, 0, 0, portMAX_DELAY);
        ESP_LOGI("NVS", "Storing current configuration to NVS");

        err = nvs_set_u32(handle, "transition", *((uint32_t*)(&(state.transition))));
        log_error_if_nonzero("writing NVS transition", err);

        err = nvs_commit(handle);
        log_error_if_nonzero("from NVS commit", err);
    }

    nvs_close(handle); // never called
}

cJSON* state_to_json() {
    cJSON *json = cJSON_CreateObject();

    if (state.color_mode == Color_Temp) {
        cJSON_AddNumberToObject(json, "color_temp", state.color_temperature);
        cJSON_AddStringToObject(json, "color_mode", "color_temp");
    } else {
        cJSON *color = cJSON_CreateObject();
        cJSON_AddNumberToObject(color, "r", state.r);
        cJSON_AddNumberToObject(color, "g", state.g);
        cJSON_AddNumberToObject(color, "b", state.b);
        //cJSON_AddNumberToObject(color, "w", state.ww);
        //cJSON_AddNumberToObject(color, "c", state.cw);

        cJSON_AddItemToObject(json, "color", color);
        cJSON_AddStringToObject(json, "color_mode", "rgb");
    }

    cJSON_AddNumberToObject(json, "brightness", state.brightness);

    if (state.state) {
        cJSON_AddStringToObject(json, "state", "ON");
    } else {
        cJSON_AddStringToObject(json, "state", "OFF");
    }

    return json;
}

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}



extern "C" esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            //
            // MQTT device discovery for HomeAssistant
            //
            {
                const char data[] = " \
{ \
    \"brightness\":true, \
    \"color_mode\":true, \
    \"command_topic\":\"esp32/" DEVICE_ID "/set\", \
    \"device\":{ \
       \"identifiers\":[ \
          \"esp32_" DEVICE_ID "\" \
       ], \
       \"manufacturer\":\"kongr45gpen\", \
       \"model\":\"ESP32 RGB light, v1\", \
       \"name\":\"" DEVICE_ID "\", \
       \"sw_version\":\"ESP32 IDF RGB controller 0.1.1\" \
    }, \
    \"json_attributes_topic\":\"esp32/" DEVICE_ID "\", \
    \"name\":\"" DEVICE_ID "\", \
    \"schema\":\"json\", \
    \"state_topic\":\"esp32/" DEVICE_ID "\", \
    \"supported_color_modes\":[ \
       \"rgb\"\
    ], \
    \"unique_id\":\"" DEVICE_ID "_light_esp32\" \
 } \
                ";

                esp_mqtt_client_publish(client, "homeassistant/light/" DEVICE_ID "/config", data, strlen(data), 1, 1);
            }

            //
            // MQTT set state to ON for HomeAssistant
            //
            {
                const char data[] = "ON";
                // TODO: Set retain to OFF (?) and re_trigger
                esp_mqtt_client_publish(client, "homeassistant/light/" DEVICE_ID "/state", data, strlen(data), 1, 1);
            }

            {
                cJSON *json = state_to_json();
                char *json_string = cJSON_Print(json);
                esp_mqtt_client_publish(client, "esp32/" DEVICE_ID, json_string, 0, 1, 0);

                cJSON_Delete(json);
                free(json_string);
            }

            esp_mqtt_client_subscribe(client, "esp32/" DEVICE_ID "/set", 1);

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            // set
            cJSON *json = cJSON_Parse(event->data);

            cJSON *json_state = cJSON_GetObjectItemCaseSensitive(json, "state");
            if (json_state) {
                if (strcmp(cJSON_GetStringValue(json_state), "OFF") == 0) {
                    state.state = 0;
                } else {
                    state.state = 1;
                }
            }

            cJSON *brightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
            if (brightness) {
                state.brightness = cJSON_GetNumberValue(brightness);
            }

            cJSON *color_mode = cJSON_GetObjectItemCaseSensitive(json, "color_mode");
            if (color_mode) {
                if (cJSON_GetStringValue(color_mode)[0] == 'R') {
                    state.color_mode = RGBWW;
                } else {
                    state.color_mode = Color_Temp;
                }
            }

            cJSON *color_temp = cJSON_GetObjectItemCaseSensitive(json, "color_temp");
            if (color_temp) {
                state.color_temperature = cJSON_GetNumberValue(color_temp);
                state.color_mode = Color_Temp;
            }

            cJSON *color = cJSON_GetObjectItemCaseSensitive(json, "color");
            if (color) {
                cJSON *r = cJSON_GetObjectItemCaseSensitive(color, "r");
                cJSON *g = cJSON_GetObjectItemCaseSensitive(color, "g");
                cJSON *b = cJSON_GetObjectItemCaseSensitive(color, "b");
                cJSON *w = cJSON_GetObjectItemCaseSensitive(color, "w");
                cJSON *c = cJSON_GetObjectItemCaseSensitive(color, "c");

                state.r = cJSON_GetNumberValue(r);
                state.g = cJSON_GetNumberValue(g);
                state.b = cJSON_GetNumberValue(b);
                state.ww = cJSON_GetNumberValue(w);
                state.cw = cJSON_GetNumberValue(c);

                state.color_mode = RGBWW;
            }

            cJSON *transition = cJSON_GetObjectItemCaseSensitive(json, "transition");
            if (transition) {
                state.transition = cJSON_GetNumberValue(transition);
            }

            cJSON_Delete(json);

            {
                cJSON *json = state_to_json();
                char *json_string = cJSON_Print(json);
                esp_mqtt_client_publish(client, "esp32/" DEVICE_ID, json_string, 0, 1, 0);

                cJSON_Delete(json);
                free(json_string);
            }

            xTaskNotify(led_task, 0, eNoAction);

            // Commit changes to NVS if needed
            if (nvs_task != NULL) {
                if (transition) {
                    xTaskNotify(nvs_task, 0, eNoAction);
                }
            }

            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb((esp_mqtt_event_handle_t) event_data);
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

    esp_mqtt_client_config_t mqtt_cfg;
    mqtt_cfg.broker.address.uri =  "mqtt://home";
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    xTaskCreate(&led_blink,"LED_BLINK",2048,NULL,5,&led_task);
    //xTaskCreate(&nvs_manager,"NVS_MANAGER",2048,NULL,3,&nvs_task);
    // vTaskStartScheduler();

}