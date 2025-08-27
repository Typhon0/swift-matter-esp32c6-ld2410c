#include "ld2410_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>


static const char *TAG = "LD2410";

// Frame constants
const uint8_t CMD_HEADER[] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t CMD_TAIL[] = {0x04, 0x03, 0x02, 0x01};
const uint8_t DATA_HEADER[] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t DATA_TAIL[] = {0xF8, 0xF7, 0xF6, 0xF5};

// Commands
const uint8_t CMD_ENABLE_CONFIG[] = {0xFF, 0x01};
const uint8_t CMD_DISABLE_CONFIG[] = {0xFE, 0x00};
const uint8_t CMD_GET_FIRMWARE[] = {0xA0, 0x00};
const uint8_t CMD_GET_MAC[] = {0xA5, 0x00};
const uint8_t CMD_ENABLE_ENHANCED[] = {0x61, 0x01};
const uint8_t CMD_DISABLE_ENHANCED[] = {0x60, 0x00};


LD2410Driver::LD2410Driver(uart_port_t uart_num, bool debug)
    : uart_num(uart_num), debug_mode(debug) {}

bool LD2410Driver::begin() {
    // Note: UART is configured and installed by the wrapper now.
    // Attempt to leave config mode, in case the sensor is stuck in it.
    return configMode(false);
}

void LD2410Driver::end() {
    // UART driver is managed by the wrapper.
}

bool LD2410Driver::sendCommand(const uint8_t *command, size_t length) {
    uart_write_bytes(uart_num, (const char *)CMD_HEADER, sizeof(CMD_HEADER));
    uint16_t cmd_len = length;
    uart_write_bytes(uart_num, (const char *)&cmd_len, sizeof(cmd_len));
    uart_write_bytes(uart_num, (const char *)command, length);
    uart_write_bytes(uart_num, (const char *)CMD_TAIL, sizeof(CMD_TAIL));
    return uart_wait_tx_done(uart_num, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool LD2410Driver::readFrame(uint8_t* buffer, size_t max_length) {
    // Do not flush here, response comes right after command
    int read_len = uart_read_bytes(uart_num, buffer, max_length, pdMS_TO_TICKS(300));

    if (debug_mode && read_len > 0) {
        ESP_LOGI(TAG, "readFrame received %d bytes:", read_len);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, read_len, ESP_LOG_INFO);
    }

    if (read_len < 12) { // Minimum ACK frame size
        if (debug_mode && read_len > 0) {
            ESP_LOGE(TAG, "ReadFrame: Incomplete frame, got %d bytes", read_len);
        }
        return false;
    }

    // Simple check for header and tail
    if (memcmp(buffer, CMD_HEADER, sizeof(CMD_HEADER)) == 0 &&
        memcmp(buffer + read_len - sizeof(CMD_TAIL), CMD_TAIL, sizeof(CMD_TAIL)) == 0) {
        return true;
    }
    
    if (debug_mode) {
        ESP_LOGE(TAG, "ReadFrame: Invalid ACK frame received.");
    }
    return false;
}

bool LD2410Driver::presenceDetected() {
    uart_flush(uart_num); // Clear any old data before checking for presence data
    uint8_t buffer[64]; // Buffer to hold incoming data
    int len = uart_read_bytes(uart_num, buffer, sizeof(buffer), pdMS_TO_TICKS(200));

    if (debug_mode && len > 0) {
        ESP_LOGI(TAG, "presenceDetected received %d bytes:", len);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_INFO);
    }

    if (len > 0) {
        for (int i = 0; i <= len - sizeof(DATA_HEADER) - sizeof(DATA_TAIL) - 1; ++i) {
            if (memcmp(buffer + i, DATA_HEADER, sizeof(DATA_HEADER)) == 0) {
                // Found data frame header
                // The state byte is at offset 8 in an engineering mode frame
                uint8_t state = buffer[i + 8];
                // state: 0x00: no target, 0x01: moving target, 0x02: still target, 0x03: moving & still
                this->status = state;
                if (debug_mode) ESP_LOGI(TAG, "Presence state: %d", state);
                return state != 0x00;
            }
        }
    }
    // If no data frame, assume no presence
    this->status = 0x00;
    if (debug_mode) ESP_LOGI(TAG, "No presence data frame found.");
    return false;
}

uint8_t LD2410Driver::getStatus() {
    // This is updated by presenceDetected()
    return this->status;
}

std::string LD2410Driver::getFirmware() {
    if (!configMode(true)) return "";
    sendCommand(CMD_GET_FIRMWARE, sizeof(CMD_GET_FIRMWARE));

    uint8_t buffer[32];
    if (readFrame(buffer, sizeof(buffer))) {
        // Response format: FD FC FB FA | 14 00 | A0 | 00 | (major) (minor) | (bug) | ...
        if (buffer[6] == 0xA0 && buffer[7] == 0x00) {
            std::ostringstream ss;
            ss << "V" << (int)buffer[8] << "." << (int)buffer[9] << "." << std::hex
               << (int)buffer[13] << (int)buffer[12] << (int)buffer[11] << (int)buffer[10];
            firmwareVersion = ss.str();
        }
    }
    configMode(false);
    return firmwareVersion;
}

std::string LD2410Driver::getMAC() {
    if (!configMode(true)) return "";
    sendCommand(CMD_GET_MAC, sizeof(CMD_GET_MAC));
    uint8_t buffer[20];
    if (readFrame(buffer, sizeof(buffer))) {
        if (buffer[6] == 0xA5 && buffer[7] == 0x00) {
            std::ostringstream ss;
            for (int i = 0; i < 6; ++i) {
                ss << byteToHex(buffer[13 - i]) << (i < 5 ? ":" : "");
            }
            return ss.str();
        }
    }
    configMode(false);
    return "";
}

bool LD2410Driver::configMode(bool enable) {
    uart_flush(uart_num);
    const uint8_t* cmd = enable ? CMD_ENABLE_CONFIG : CMD_DISABLE_CONFIG;
    const uint8_t cmd_val = enable ? 0xFF : 0xFE;
    
    sendCommand(cmd, 2); // Command is 2 bytes long
    
    uint8_t ack[20];
    if (readFrame(ack, sizeof(ack))) {
        // Correct ACK: Header | Len=2 | 0x01 | cmd_val | Status=0 | Tail
        if (ack[4] == 0x02 && ack[5] == 0x00 && ack[6] == 0x01 && ack[7] == cmd_val && ack[8] == 0x00) {
            isConfigMode = enable;
            if (debug_mode) ESP_LOGI(TAG, "Config mode %s successfully.", enable ? "enabled" : "disabled");
            vTaskDelay(pdMS_TO_TICKS(200)); // Give it time to switch modes
            return true;
        }
    }
    if (debug_mode) ESP_LOGE(TAG, "Failed to %s config mode.", enable ? "enable" : "disable");
    return false;
}

bool LD2410Driver::enhancedMode(bool enable) {
    if (!configMode(true)) return false;
    const uint8_t* cmd = enable ? CMD_ENABLE_ENHANCED : CMD_DISABLE_ENHANCED;
    sendCommand(cmd, sizeof(CMD_ENABLE_ENHANCED));

    uint8_t ack[12];
    bool success = false;
    if (readFrame(ack, sizeof(ack))) {
        if (ack[6] == (enable ? 0x61 : 0x60) && ack[7] == 0x00 && ack[9] == 0x00) {
            if (debug_mode) ESP_LOGI(TAG, "Enhanced mode %s successfully.", enable ? "enabled" : "disabled");
            success = true;
        }
    }
    if (!success && debug_mode) ESP_LOGE(TAG, "Failed to %s enhanced mode.", enable ? "enable" : "disable");

    configMode(false);
    return success;
}

std::string LD2410Driver::byteToHex(uint8_t byte) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setw(2)
       << std::setfill('0') << static_cast<int>(byte);
    return ss.str();
}