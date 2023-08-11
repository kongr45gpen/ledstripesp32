#include <string>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "config.h"
#include "config.hpp"
#include "nvs_flash.h"

using json = nlohmann::json;

static const char *TAG = "Home App MQTT";

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

extern TaskHandle_t led_task;

void log_error_if_nonzero(const char * message, int error_code);

json state_to_json() {
    json json_state;

    if (state.color_mode == Color_Temp) {
        json_state["color_temp"] = state.color_temperature;
        json_state["color_mode"] = "color_temp";
    } else {
        json_state["color"] = {
            { "r", state.r, },
            { "g", state.g, },
            { "b", state.b, },
            // { "w", state.ww, },
            // { "c", state.cw, },
        };
    }

    json_state["brightness"] = state.brightness;
    json_state["state"] = state.state ? "ON" : "OFF";

    return json_state;
}


esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
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
                std::string json_string = state_to_json().dump();
                esp_mqtt_client_publish(client, "esp32/" DEVICE_ID, json_string.c_str(), 0, 1, 0);
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
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

            // TODO: A better way to catch errors here (e.g. incompatible types)

            json j = json::parse(event->data, event->data + event->data_len, nullptr, false);
            if (j.is_discarded()) {
                ESP_LOGE(TAG, "JSON parsing error. Please provide correct JSON next time.");
                return ESP_OK;
            }

            if (j.contains("state")) {
                if (j["state"] == "OFF") {
                    state.state = 0;
                } else {
                    state.state = 1;
                }
            }

            if (j.contains("brightness")) {
                state.brightness = j["brightness"];
            }

            if (j.contains("color_mode")) {
                if (j["color_mode"] == "color_temp") {
                    state.color_mode = Color_Temp;
                } else {
                    state.color_mode = RGBWW;
                }
            }

            if (j.contains("color_temp")) {
                state.color_temperature = j["color_temp"];
                state.color_mode = Color_Temp;
            }

            if (j.contains("color")) {
                state.r = j["color"]["r"];
                state.g = j["color"]["g"];
                state.b = j["color"]["b"];
                // state.ww = j["color"]["w"];
                // state.cw = j["color"]["c"];

                state.color_mode = RGBWW;
            }

            if (j.contains("transition")) {
                state.transition = j["transition"];
            }

            std::string json_string = state_to_json().dump();
            esp_mqtt_client_publish(client, "esp32/" DEVICE_ID, json_string.c_str(), 0, 1, 0);

            xTaskNotify(led_task, 0, eNoAction);

            // Commit changes to NVS if needed
            // if (nvs_task != NULL) {
            //     if (transition) {
            //         xTaskNotify(nvs_task, 0, eNoAction);
            //     }
            // }

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

void mqtt_connect() {
    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.broker.address.uri =  "mqtt://home";
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}