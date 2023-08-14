#include "Light.hpp"
#include "tasks.h"
#include "main.hpp"
#include "esp_wifi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void led_blink(void *pvParams) {
    for (auto &light : Light::all_lights) {
        light.second.initialise();
    }

    while (true) {
        for (auto &light : Light::all_lights) {
            // TODO: Only render lights that have changes
            light.second.render();
        }

        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
    }
}

/**
 * Reimplementation of progress bar with slightly different design
 */
void draw_progress_bar(OLEDDisplay& display, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
  display.setColor(WHITE);
  display.drawHorizontalLine(x, y, width + 1);
  display.drawHorizontalLine(x, y + height, width + 1);
  display.drawVerticalLine(x, y, height + 1);
  display.drawVerticalLine(x + width, y, height + 1);

  uint16_t maxProgressWidth = (width - 3) * progress / 255;

  if (progress > 0) {
    display.fillRect(x + 2, y + 2, maxProgressWidth, height - 3);
  }
}

void draw_progress_bar_circle(OLEDDisplay& display, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = 2 * radius;
  uint16_t innerRadius = radius - 2;

  display.setColor(WHITE);
  display.drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
  display.drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  display.drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  display.drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

  uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 255;

  if (progress > 0) {
    display.fillCircle(xRadius, yRadius, innerRadius);
    display.fillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
    display.fillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
  }
}

void display_update(void *pvParams) {
    static auto hostname = config->hostname();
    static std::string ssid = std::string(config->ssid());

    if (ssid.length() > 8) {
        ssid = ssid.substr(0, 8);
    }

    // static int rssi = 0;
    // static char rssi_str[10] = "???";

    while (true) {
        if (display) {
            display->clear();
            display->setTextAlignment(TEXT_ALIGN_LEFT);
            display->setFont(ArialMT_Plain_16);
            display->drawString(0, 0, hostname.c_str());

            display->setFont(ArialMT_Plain_10);
            display->setTextAlignment(TEXT_ALIGN_RIGHT);
            display->drawString(127, 0, ssid.c_str());
            // Not yet implemented in ESP-IDF!
            // if (esp_wifi_sta_get_rssi(&rssi) == ESP_OK) {
            //     snprintf(rssi_str, 10, "%d dBm", rssi);
            // } else {
            //     snprintf(rssi_str, 10, "ERR!");
            // }
            // display->setFont(ArialMT_Plain_16);
            // display->drawString(127, 0, rssi_str);

            display->drawHorizontalLine(0, 19, 128);

            int coord_y = 22;
            int i = 0;
            int j = 0;
            for (auto &light : Light::all_lights) {
                display->setTextAlignment(TEXT_ALIGN_RIGHT);
                display->setFont(ArialMT_Plain_10);
                display->drawString(30, coord_y, light.first.c_str());

                const auto& brightness = light.second.get_current_brightness();

                for (int pin = 0; pin < light.second.count_pins(); pin++) {
                    display->setTextAlignment(TEXT_ALIGN_LEFT);
                    display->setFont(ArialMT_Plain_10);
                    // draw_progress_bar_circle(*display, 34 + i * 32, coord_y, 28, 12, brightness[pin].load());
                    draw_progress_bar(*display, 34 + i * 32, coord_y, 26, 12, brightness[pin].load());

                    i += 1;
                    if (i >= 3) {
                        i = 0;
                        j += 1;
                        coord_y += 16;
                    }
                }
            }

            display->display();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}