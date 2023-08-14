#pragma once

#include <optional>
#include "config.hpp"
#include "SSD1306.hpp"

static const char *TAG = "Home App";

extern std::optional<Configuration> config;
extern std::optional<SSD1306IDF> display;
