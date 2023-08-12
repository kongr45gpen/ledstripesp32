#include "esp_log.h"
#include "config.hpp"
#include "Light.hpp"

extern const uint8_t config_json[]   asm("config_json");

using json = nlohmann::json;

Configuration read_config() {
    ESP_LOGD("CONFIG", "Config input is %s", config_json);
    ESP_LOGI("CONFIG", "Reading config...");

    auto config = Configuration(json::parse(config_json));

    ESP_LOGI("CONFIG", "Config is %s", config.dump().c_str());
    
    return config;

}
