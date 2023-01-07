#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/ledc.h"


#define DEVICE_ID "qMHSxIpfAz2001ll"
#define LED_PIN 4

const uart_port_t uart_num = UART_NUM_2;

ledc_channel_config_t ledc_channel[1] = {
        {
            .channel = LEDC_CHANNEL_0,
            .duty = 0,
            .gpio_num = LED_PIN,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint = 0,
            .timer_sel = LEDC_TIMER_0,
            // .flags.output_invert = 0
        }
    };

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
};

struct State state = {
    .state = 0,
    .brightness = 100,
    .r = 255,
    .g = 255,
    .b = 255,
    .ww = 0,
    .cw = 0,
    .color_temperature = 400,
    .color_mode = Color_Temp
};

void led_blink(void *pvParams) {
    ESP_LOGI("MAIN", "Configuring LED timer...");

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_12_BIT, // resolution of PWM duty
        .freq_hz = 5000,                      // frequency of PWM signal
        .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
        .timer_num = LEDC_TIMER_0,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ESP_LOGI("LED", "LED timer configured");



    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[0]));

    ESP_LOGI("MAIN", "LED channel configured");

    uint16_t duty = 0;
    while(1) {
        //duty = (duty + 10) % (2 << 12);
        //duty = 3;
        duty = state.brightness * 16;

        ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, duty);
        ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
        vTaskDelay(1);
    }

    // gpio_pad_select_gpio(LED_PIN);
    // gpio_set_direction (LED_PIN,GPIO_MODE_OUTPUT);
    // while (1) {
    //     // char* test_str = "blinkomania.\n";
    //     // uart_write_bytes(uart_num, (const char*)test_str, strlen(test_str));

    //     ESP_LOGI("MAIN", "Time to log and blink!");

    //     gpio_set_level(LED_PIN,0);
    //     vTaskDelay(1000/portTICK_RATE_MS);
    //     gpio_set_level(LED_PIN,1);
    //     vTaskDelay(1000/portTICK_RATE_MS);
    // }
}

#define EXAMPLE_ESP_WIFI_SSID      "NETWORK_NAME"
#define EXAMPLE_ESP_WIFI_PASS      "NETWORK_PASS"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

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
};

struct State state = {
    .state = 0,
    .brightness = 100,
    .r = 255,
    .g = 255,
    .b = 255,
    .ww = 0,
    .cw = 0,
    .color_temperature = 400,
    .color_mode = Color_Temp
};

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
        cJSON_AddNumberToObject(color, "w", state.ww);
        cJSON_AddNumberToObject(color, "c", state.cw);

        cJSON_AddItemToObject(json, "color", color);
        cJSON_AddStringToObject(json, "color_mode", "rgbww");
    }

    cJSON_AddNumberToObject(json, "brightness", state.brightness);

    if (state.state) {
        cJSON_AddStringToObject(json, "state", "ON");
    } else {
        cJSON_AddStringToObject(json, "state", "OFF");
    }

    return json;
}


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

#define DEFAULT_SCAN_LIST_SIZE 30

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

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }

}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
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
       \"model\":\"ESP32 RGBWW light, v1\", \
       \"name\":\"" DEVICE_ID "\", \
       \"sw_version\":\"ESP32 IDF RGB controller 0.1.0\" \
    }, \
    \"json_attributes_topic\":\"esp32/" DEVICE_ID "\", \
    \"max_mireds\":500, \
    \"min_mireds\":150, \
    \"name\":\"" DEVICE_ID "\", \
    \"schema\":\"json\", \
    \"state_topic\":\"esp32/" DEVICE_ID "\", \
    \"supported_color_modes\":[ \
       \"color_temp\", \
       \"rgbww\" \
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
        case MQTT_EVENT_DATA:
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

            cJSON_Delete(json);

            {
                cJSON *json = state_to_json();
                char *json_string = cJSON_Print(json);
                esp_mqtt_client_publish(client, "esp32/" DEVICE_ID, json_string, 0, 1, 0);

                cJSON_Delete(json);
                free(json_string);
            }

            {
                ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, state.brightness / 256.0 * ((1 << 12) - 1));
                ESP_LOGI("LED", "Set brightness to %f", state.brightness / 256.0 * ((1 << 12) - 1));
                ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
            }

            break;
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
    mqtt_event_handler_cb(event_data);
}

void app_main() {
    esp_log_level_set("*", ESP_LOG_DEBUG);      // enable WARN logs from WiFi stack

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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

    const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtt://home",
        // .user_context = (void *)your_context
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    xTaskCreate(&led_blink,"LED_BLINK",2048,NULL,5,NULL);
    // vTaskStartScheduler();
}