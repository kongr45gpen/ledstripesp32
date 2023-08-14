#pragma once

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <utility>

#ifdef CONFIG_IDF_TARGET
#define JSON_TRY_USER if(true)
#define JSON_CATCH_USER(exception) if(false)
#define JSON_THROW_USER(exception)                           \
    {\
        ESP_LOGE("JSON", "Error in %s:%d (function %s) - %s", __FILE__, __LINE__, __FUNCTION__, (exception).what());\
        vTaskDelay(100);\
        abort();\
    }
#endif

#include "nlohmann/json.hpp"

class Configuration {
    nlohmann::json json;
public:
    explicit Configuration(const nlohmann::json& json) : json(json) {}

    wifi_config_t get_wifi_config() {
        std::string ssid = json.at("wifi").at("ssid");
        std::string password = json.at("wifi").at("password");

        wifi_config_t wifi_config = {};

        if (ssid.length() > sizeof(wifi_config.sta.ssid)) {
            ESP_LOGE("CONFIG", "SSID is too long");
            abort();
        }

        if (password.length() > sizeof(wifi_config.sta.password)) {
            ESP_LOGE("CONFIG", "Password is too long");
            abort();
        }
        
        strcpy((char*)wifi_config.sta.ssid, ssid.c_str());
        strcpy((char*)wifi_config.sta.password, password.c_str());

        wifi_auth_mode_t auth_mode;
        std::string auth_string = json.at("wifi").value("auth", "null");
        if (auth_string == "OPEN") {
            auth_mode = WIFI_AUTH_OPEN;
        } else if (auth_string == "WEP") {
            auth_mode = WIFI_AUTH_WEP;
        } else if (auth_string == "WPA_PSK") {
            auth_mode = WIFI_AUTH_WPA_PSK;
        } else if (auth_string == "WPA2_PSK") {
            auth_mode = WIFI_AUTH_WPA2_PSK;
        } else if (auth_string == "WPA_WPA2_PSK") {
            auth_mode = WIFI_AUTH_WPA_WPA2_PSK;
        } else if (auth_string == "WPA2_ENTERPRISE") {
            auth_mode = WIFI_AUTH_WPA2_ENTERPRISE;
        } else if (auth_string == "WPA3_PSK") {
            auth_mode = WIFI_AUTH_WPA3_PSK;
        } else if (auth_string == "WPA2_WPA3_PSK") {
            auth_mode = WIFI_AUTH_WPA2_WPA3_PSK;
        } else if (auth_string == "WIFI_AUTH_WAPI_PSK") {
            auth_mode = WIFI_AUTH_WAPI_PSK;
        } else {
            ESP_LOGE("CONFIG", "Invalid wifi authentication mode. Use one of OPEN, WEP, WPA_PSK, WPA2_PSK, WPA_WPA2_PSK, WPA2_ENTERPRISE, WPA3_PSK, WPA2_WPA3_PSK, WIFI_AUTH_WAPI_PSK");
            auth_mode = WIFI_AUTH_OPEN;
        }

        wifi_config.sta.threshold.authmode = auth_mode;
        
        return wifi_config;
    }

    std::string_view ssid() {
        return json.at("wifi").at("ssid").get<std::string_view>();
    }

    std::string hostname() {
        return json.at("wifi").value("hostname", "espressif");
    }

    std::string unique_id() {
        if (json.at("wifi").contains("hostname")) {
            return json["wifi"]["hostname"];
        } else {
            uint8_t mac[6];
            char mac_str[30];
            esp_efuse_mac_get_default(mac);
            sprintf(mac_str, "esp32_led_%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2],
                    mac[3], mac[4], mac[5]);
            return std::string(mac_str);
        }
    }

    std::string mqtt_host() {
        return json.at("mqtt").at("host");
    }

    nlohmann::json::reference lights() {
        return json.at("lights");
    }

    std::string dump() {
        return json.dump();
    }

    bool has_display() {
        return json.contains("display");
    }

    i2c_config_t get_display_i2c_config() {
        i2c_config_t conf;
        memset(&conf, 0, sizeof(conf));
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = json.at("display").at("sda");
        conf.scl_io_num = json.at("display").at("scl");
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = 100000;
        return conf;
    }
};


Configuration read_config();
