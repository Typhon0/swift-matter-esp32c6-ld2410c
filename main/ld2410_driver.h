#pragma once
#include "driver/uart.h"
#include <string>

/**
 * @brief Controler pentru senzorul de prezență LD2410 (ESP-IDF)
 */
class LD2410Driver {
public:
    LD2410Driver(uart_port_t uart_num, bool debug = false);
    bool begin();
    void end();
    bool presenceDetected();
    uint8_t getStatus();
    std::string getFirmware();
    std::string getMAC();
    bool configMode(bool enable);
    bool enhancedMode(bool enable);

private:
    uart_port_t uart_num;
    bool debug_mode;
    bool sendCommand(const uint8_t* command, size_t length);
    bool readFrame(uint8_t* buffer, size_t length);
    std::string byteToHex(uint8_t byte);
    bool isConfigMode = false;
    uint8_t mac[6];
    std::string firmwareVersion;
    uint8_t status = 0xFF;
};