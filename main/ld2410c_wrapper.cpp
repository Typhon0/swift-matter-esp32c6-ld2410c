#include "ld2410c_wrapper.h"
#include "ld2410_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Using UART1, but this can be changed.
// Make sure to connect the LD2410C sensor to the correct pins on the ESP32-C6.
// Default pins for UART1 on many ESP32-C6 boards are:
// TX: GPIO2
// RX: GPIO3
#define LD2410_UART_NUM UART_NUM_1
#define LD2410_TX_PIN 2
#define LD2410_RX_PIN 3

static const char *TAG_WRAPPER = "ld2410c_wrapper";
static LD2410Driver* ld2410_sensor = nullptr;

void ld2410c_init() {
    ESP_LOGI(TAG_WRAPPER, "Initializing LD2410C sensor driver.");

    // Give the sensor a moment to power up fully before communication.
    vTaskDelay(pdMS_TO_TICKS(500));

    // Initialize the UART driver
    uart_config_t uart_config = {
        .baud_rate = 256000,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(LD2410_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LD2410_UART_NUM, LD2410_TX_PIN, LD2410_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(LD2410_UART_NUM, 256, 0, 0, NULL, 0));

    ld2410_sensor = new LD2410Driver(LD2410_UART_NUM, true);
    if (!ld2410_sensor->begin()) {
        ESP_LOGW(TAG_WRAPPER, "LD2410C sensor did not acknowledge exit config mode. This is often normal on startup. Continuing...");
    }

    ESP_LOGI(TAG_WRAPPER, "LD2410C sensor initialized.");
    std::string fw = ld2410_sensor->getFirmware();
    if (!fw.empty()) {
        ESP_LOGI(TAG_WRAPPER, "LD2410C Firmware: %s", fw.c_str());
    } else {
        ESP_LOGW(TAG_WRAPPER, "Could not read LD2410C firmware version.");
    }
}

void ld2410c_poll() {
    // The driver implementation reads data on demand in presenceDetected(),
    // so a separate polling function is not strictly necessary for this driver design.
    // This function is kept for API compatibility with the Swift code.
}

bool ld2410c_is_present() {
    if (ld2410_sensor) {
        return ld2410_sensor->presenceDetected();
    }
    ESP_LOGW(TAG_WRAPPER, "ld2410c_is_present() called before initialization.");
    return false;
}

