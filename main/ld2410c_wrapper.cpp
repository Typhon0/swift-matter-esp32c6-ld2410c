#include "ld2410c_wrapper.h"
#include "ld2410_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

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
static uint32_t ld2410_init_time_ms = 0;
static char ld2410_fw_str[32] = {0};

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
    ld2410_init_time_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    std::string fw = ld2410_sensor->getFirmware();
    if (!fw.empty()) {
        ESP_LOGI(TAG_WRAPPER, "LD2410C Firmware: %s", fw.c_str());
    strncpy(ld2410_fw_str, fw.c_str(), sizeof(ld2410_fw_str) - 1);
    } else {
        ESP_LOGW(TAG_WRAPPER, "Could not read LD2410C firmware version.");
    }
}

void ld2410c_poll() {
    // Trigger a lightweight read to update internal buffers by asking for presence.
    if (ld2410_sensor) {
        // Read any pending UART frames (ACK or DATA). This updates internal status.
        ld2410_sensor->check();
        bool present = ld2410_sensor->presenceDetected();
        static uint8_t lastStatus = 0xFF;
        static bool warnedNoData = false;
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        uint8_t st = ld2410_sensor->getStatus();
        if (st != lastStatus) {
            switch (st) {
                case 0: ESP_LOGI(TAG_WRAPPER, "State: 0 No target"); break;
                case 1: ESP_LOGI(TAG_WRAPPER, "State: 1 Moving only"); break;
                case 2: ESP_LOGI(TAG_WRAPPER, "State: 2 Stationary only"); break;
                case 3: ESP_LOGI(TAG_WRAPPER, "State: 3 Moving & Stationary"); break;
                case 4: ESP_LOGI(TAG_WRAPPER, "State: 4 Auto thresholds in progress"); break;
                case 5: ESP_LOGI(TAG_WRAPPER, "State: 5 Auto thresholds success"); break;
                case 6: ESP_LOGI(TAG_WRAPPER, "State: 6 Auto thresholds failed"); break;
                default: ESP_LOGI(TAG_WRAPPER, "State: 0x%02X Invalid/Expired", st); break;
            }
            lastStatus = st;
        }
        // If no valid data yet (status 0xFF) for > 3000 ms after init, warn once.
        if (st == 0xFF && !warnedNoData && (now_ms - ld2410_init_time_ms) > 3000) {
            ESP_LOGW(TAG_WRAPPER, "No LD2410C data frames received yet (status 0xFF). Check wiring, power, baud (256000), and TX/RX pins (TX GPIO2 -> sensor RX, RX GPIO3 -> sensor TX).");
            warnedNoData = true;
        }
        (void)present; // present is derived from status

        // Publish vendor telemetry periodically (simple change detection)
        static uint32_t last_publish_ms = 0;
        const uint32_t publish_interval_ms = 250; // throttle
        if (now_ms - last_publish_ms >= publish_interval_ms) {
            // Avoid publishing before Matter dynamic endpoint fully registered
            if ((now_ms - ld2410_init_time_ms) < 4000) {
                return;
            }
            last_publish_ms = now_ms;
            const LD2410Driver::SensorData &sd = ld2410_sensor->getSensorData();
            // Scalars
            ld2410c_update_vendor_scalars(
                (uint16_t)ld2410_sensor->movingTargetDistance(),
                ld2410_sensor->movingTargetSignal(),
                (uint16_t)ld2410_sensor->stationaryTargetDistance(),
                ld2410_sensor->stationaryTargetSignal(),
                (uint16_t)ld2410_sensor->detectedDistance(),
                ld2410_sensor->inEnhancedMode(),
                (uint16_t)ld2410_sensor->getRange_cm(),
                ld2410_sensor->getLightLevel(),
                ld2410_sensor->getLightThreshold(),
                ld2410_sensor->getOutLevel(),
                (uint8_t)ld2410_sensor->getAutoStatus()
            );

            // Arrays (signals & thresholds) only if enhanced mode
            if (ld2410_sensor->inEnhancedMode()) {
                const auto &mvSig = ld2410_sensor->getMovingSignals();
                const auto &stSig = ld2410_sensor->getStationarySignals();
                const auto &mvThr = ld2410_sensor->getMovingThresholds();
                const auto &stThr = ld2410_sensor->getStationaryThresholds();
                ld2410c_update_vendor_arrays(
                    mvSig.values, mvSig.N,
                    stSig.values, stSig.N,
                    mvThr.values, mvThr.N,
                    stThr.values, stThr.N,
                    ld2410_fw_str[0] ? ld2410_fw_str : nullptr
                );
            }
        }
    }
}

bool ld2410c_is_present() {
    if (ld2410_sensor) {
        return ld2410_sensor->presenceDetected();
    }
    ESP_LOGW(TAG_WRAPPER, "ld2410c_is_present() called before initialization.");
    return false;
}

uint8_t ld2410c_status() {
    if (ld2410_sensor) return ld2410_sensor->getStatus();
    return 0xFF;
}

