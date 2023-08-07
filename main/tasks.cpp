#include "Light.hpp"
#include "tasks.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern State state;

extern "C" void led_blink(void *pvParams) {
    Light<3> light({LED_PIN, LED2_PIN, LED3_PIN}, {Colour::Red, Colour::Green, Colour::Blue});

    light.initialise();

    while (true) {
        light.render(state);

        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
    }
}