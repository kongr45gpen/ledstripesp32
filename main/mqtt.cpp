#include <string>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "config.h"
#include "config.hpp"
#include "nvs_flash.h"
#include "main.hpp"
#include "Light.hpp"

using json = nlohmann::json;


extern TaskHandle_t led_task;

void log_error_if_nonzero(const char * message, int error_code);

json state_to_json(const Light& light) {
    const auto& state = light.state;

    json json_state;

    if (state.color_mode == Color_Temp) {
        json_state["color_temp"] = state.color_temperature;
        json_state["color_mode"] = "color_temp";
    } else if (state.color_mode == RGB) {
        json_state["color"] = {
            { "r", state.r, },
            { "g", state.g, },
            { "b", state.b, },
        };
        json_state["color_mode"] = "rgb";
    } else if (state.color_mode == RGBW) {
        json_state["color"] = {
            { "r", state.r, },
            { "g", state.g, },
            { "b", state.b, },
            { "w", state.ww, },
        };
        json_state["color_mode"] = "rgbw";
    } else if (state.color_mode == RGBWWCW) {
        json_state["color"] = {
            { "r", state.r, },
            { "g", state.g, },
            { "b", state.b, },
            { "w", state.ww, },
            { "c", state.cw, },
        };
        json_state["color_mode"] = "rgbww";
    } else if (state.color_mode == Brightness) {
        json_state["color_mode"] = "brightness";
    } else {
        ESP_LOGW(TAG, "Unknown color mode %d", state.color_mode);
    }

    json_state["brightness"] = state.brightness;
    json_state["state"] = state.state ? "ON" : "OFF";

    return json_state;
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    using namespace std::literals;

    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    const char data[] = "ON";
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            // MQTT device discovery for HomeAssistant
            for (const auto& [key, light] : Light::all_lights) {
                auto homeassistant_topic = std::string("homeassistant/light/") + key + "/config";
                esp_mqtt_client_publish(client, homeassistant_topic.c_str(), light.get_homeassistant_configuration().dump().c_str(), 0, 1, 1);

                auto state_topic = std::string("esp32/") + key;
                esp_mqtt_client_publish(client, state_topic.c_str(), state_to_json(light).dump().c_str(), 0, 1, 0);

                auto command_topic = std::string("esp32/") + key + "/set";
                esp_mqtt_client_subscribe(client, command_topic.c_str(), 1);
            }

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

            std::string_view topic(event->topic, event->topic_len);

            // TODO: A better way to catch errors here (e.g. incompatible types)

            json j = json::parse(event->data, event->data + event->data_len, nullptr, false);
            if (j.is_discarded()) {
                ESP_LOGE(TAG, "JSON parsing error. Please provide correct JSON next time.");
                return ESP_OK;
            }

            std::string_view query(topic);
            query.remove_prefix(6); // remove "esp32/"
            query.remove_suffix(4); // remove "/set"

            auto light = Light::all_lights.find(std::string(query));

            if (light == Light::all_lights.end()) {
                ESP_LOGE(TAG, "Light not found: %s", query.data());
                return ESP_OK;
            }

            auto& state = light->second.state;

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
                } else if (j["color_mode"] == "rgb") {
                    state.color_mode = RGB;
                } else if (j["color_mode"] == "rgbw") {
                    state.color_mode = RGBW;
                } else if (j["color_mode"] == "rgbww") {
                    state.color_mode = RGBWWCW;
                } else if (j["color_mode"] == "brightness") {
                    state.color_mode = Brightness;
                } else {
                    ESP_LOGW(TAG, "Unknown color mode received in MQTT: %s", j["color_mode"].get<std::string>().c_str());
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
                state.color_mode = RGB;

                if (j["color"].contains("w")) {
                    state.ww = j["color"]["w"];
                    state.color_mode = RGBW;
                }
                if (j["color"].contains("c")) {
                    state.cw = j["color"]["c"];
                    state.color_mode = RGBWWCW;
                }
            }

            if (j.contains("transition")) {
                state.transition = j["transition"];
            }

            std::string json_string = state_to_json(light->second).dump();
            ESP_LOGD("LED", "JSON: %s", json_string.c_str());
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
    mqtt_cfg.broker.address.uri =  config->mqtt_host().c_str();
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}