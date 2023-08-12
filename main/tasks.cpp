#include "Light.hpp"
#include "tasks.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" void led_blink(void *pvParams) {
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