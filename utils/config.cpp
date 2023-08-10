#include "nlohmann/json.hpp"
#include "config.hpp"

using json = nlohmann::json;

Configuration parse_config(const std::string& config_json) {
    json config = json::parse(config_json);

    return config;
}
