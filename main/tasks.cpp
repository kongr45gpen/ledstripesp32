#include "Light.hpp"
#include "tasks.h"
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

void display_update(void *pvParams) {
    while (true) {
        vTaskDelay(1000);
    }
}