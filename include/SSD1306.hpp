#pragma once

#include "OLEDDisplay.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <vector>

class SSD1306IDF : public OLEDDisplay {
    i2c_port_t i2c_port;
    uint8_t i2c_address;
    std::vector<uint8_t> secondBuffer;

  public:
    SSD1306IDF(i2c_port_t i2c_port, uint8_t i2c_address = 0x3c, OLEDDISPLAY_GEOMETRY geometry = GEOMETRY_128_64)
        : i2c_port(i2c_port), i2c_address(i2c_address) {
        setGeometry(geometry);

        secondBuffer.resize(displayBufferSize + 1);
        secondBuffer[0] = 0x40;
    }

    bool connect() {
        return true;
    }

    void display(void) {
        const int x_offset = (128 - this->width()) / 2;
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
        uint8_t minBoundY = UINT8_MAX;
        uint8_t maxBoundY = 0;

        uint8_t minBoundX = UINT8_MAX;
        uint8_t maxBoundX = 0;
        uint8_t x, y;

        // Calculate the Y bounding box of changes
        // and copy buffer[pos] to buffer_back[pos];
        for (y = 0; y < (this->height() / 8); y++) {
            for (x = 0; x < this->width(); x++) {
                uint16_t pos = x + y * this->width();
                if (buffer[pos] != buffer_back[pos]) {
                    minBoundY = std::min(minBoundY, y);
                    maxBoundY = std::max(maxBoundY, y);
                    minBoundX = std::min(minBoundX, x);
                    maxBoundX = std::max(maxBoundX, x);
                }
                buffer_back[pos] = buffer[pos];
            }
            taskYIELD();
        }

        // If the minBoundY wasn't updated
        // we can savely assume that buffer_back[pos] == buffer[pos]
        // holdes true for all values of pos

        if (minBoundY == UINT8_MAX) {
            return;
        }

        sendCommand(COLUMNADDR);
        sendCommand(x_offset + minBoundX);
        sendCommand(x_offset + maxBoundX);

        sendCommand(PAGEADDR);
        sendCommand(minBoundY);
        sendCommand(maxBoundY);

        uint16_t k = 0;
        for (y = minBoundY; y <= maxBoundY; y++) {
            for (x = minBoundX; x <= maxBoundX; x++) {
                secondBuffer[1 + (k++)] = buffer[x + y * this->width()];
            }
            taskYIELD();
        }
        auto status = i2c_master_write_to_device(i2c_port, i2c_address, secondBuffer.data(), k + 1, 100);
        if (status != ESP_OK) {
            ESP_LOGW("SSD1306", "Failed to send I2C data to screen: %d", status);
        }
#else
        sendCommand(COLUMNADDR);
        sendCommand(x_offset);
        sendCommand(x_offset + (this->width() - 1));

        sendCommand(PAGEADDR);
        sendCommand(0x0);

        auto status = i2c_master_write_to_device(i2c_port, i2c_address, buffer - 1, displayBufferSize, 100);
        if (status != ESP_OK) {
            ESP_LOGW("SSD1306", "Failed to send I2C data to screen: %d", status);
        }
#endif
    }

protected:
    int getBufferOffset(void) {
		return 4;
	}

  private:
    inline void sendCommand(uint8_t command) __attribute__((always_inline)) {
        uint8_t write_buf[2] = {0x80, command};
        auto status = i2c_master_write_to_device(i2c_port, i2c_address, write_buf, 2, 100);
        if (status != ESP_OK) {
            ESP_LOGW("SSD1306", "Failed to send I2C command to screen: %d", status);
        }
    }
};