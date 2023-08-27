# LED strips for ESP32

`ledstripesp32` is an ESP-IDF project that lets you control LED lights from your ESP32 and works with the following integrations:

- [MQTT](https://mqtt.org/)
- [HomeAssistant MQTT integration](https://www.home-assistant.io/integrations/light.mqtt/)
- ... more to come

## Hardware

The project is built for the **ESP32-C6** SoC, and mostly tested with the [**ESP32-C6-DevKitC-1**](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/index.html) board. Other ESP32 boards should work as well in theory.

It is directly compatible with the [led-controller-pcb](https://github.com/kongr45gpen/led-controller-pcb/) board.

## Installation

This has been tested mostly on Linux (or WSL with [usbpid](https://github.com/dorssel/usbipd-win)).

1. Install the [ESP-IDF framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. Make sure to run `export.sh` from ESP-IDF before working with this project
3. Clone the project:
    ```bash
    git clone --recursive https://github.com/kongr45gpen/ledstripesp32.git
    cd ledstripesp32
    ```
4. Select the `sdkconfig` file you want to use, or create your own:
    ```bash
    cp sdkconfig.esp32c6 sdkconfig
    ```
5. Create a `config.json` file based on `config.example.json`:
    ```bash
    cp config.example.json config.json
    editor config.json
    ```
5. Build the project!
    ```bash
    idf.py build
    ```
6. Flash the project to your ESP32:
    ```bash
    idf.py flash # or idf.py flash monitor to show the log output
    ```

## Configuration
Configuration happens through the `config.json` file. You can see `config.example.json` for an example.

The following top-level configuration options are supported:
- **`wifi`**: Configure the connection details to your WiFi network
- **`mqtt`**: Configure the connection details to your MQTT broker
- **`lights`**: Configure all lights connected to the ESP32. Multiple lights can be configured here, each with an arbitrary name.
- **`display`**: Configure a connected SSD1036 display (optional). If a display is not connected, it will be ignored.

See [`include/config.hpp`](/include/config.hpp) to take a peek into configuration values.

:information_source: The JSON configuration file supports **comments** for documentation purposes, even though this is not part of the JSON specification. See [nlohmann/json](https://json.nlohmann.me/features/comments/) for more details.

:warning: There is currently no method to re-upload the configuration file after flashing. You will need to rebuild the proejct.

## Questions

**Q**: What is the purpose of this project, when alternatives like [ESPHome](https://esphome.io/index.html) exist?  
**A**: `ledstripesp32` is built for more advanced lighting control. Features such as individual control of addressable LEDs, effects, color mixing, and compatibility with protocols such as Zigbee, DMX, UART and more are planned. It is focused specifically around lighting for professional use and around a specific chip.