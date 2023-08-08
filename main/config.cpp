#include "esp_log.h"
#include "config.hpp"

#define JSON_TRY_USER if(true)
#define JSON_CATCH_USER(exception) if(false)
#define JSON_THROW_USER(exception)                           \
    {\
        ESP_LOGE("JSON", "Error in %s:%d (function %s) - %s", __FILE__, __LINE__, __FUNCTION__, (exception).what());\
        abort();\
    }

#include "nlohmann/json.hpp"

extern const uint8_t config_json[]   asm("config_json");

using json = nlohmann::json;

void read_config() {
    ESP_LOGI("CONFIG", "Reading config...");

    json config = json::parse(config_json);

    ESP_LOGI("CONFIG", "Config is %s", config.dump().c_str());
}
