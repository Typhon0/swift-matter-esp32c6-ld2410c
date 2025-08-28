// Port of Arduino MyLD2410 library (https://github.com/iavorvel/MyLD2410)
// to ESP-IDF C++ (Stream -> UART driver). Provides a largely identical API.

#pragma once
#include "driver/uart.h"
#include <cstdint>
#include <string>
#include <array>

// Baud rate defined by original library
#define LD2410_BAUD_RATE 256000
#define LD2410_BUFFER_SIZE 0x40

enum class LightControl : int8_t {
    NOT_SET = -1,
    NO_LIGHT_CONTROL = 0,
    LIGHT_BELOW_THRESHOLD = 1,
    LIGHT_ABOVE_THRESHOLD = 2
};

enum class OutputControl : int8_t {
    NOT_SET = -1,
    DEFAULT_LOW = 0,
    DEFAULT_HIGH = 1,
};

enum class AutoStatus : int8_t {
    NOT_SET = -1,
    NOT_IN_PROGRESS = 0,
    IN_PROGRESS = 1,
    COMPLETED = 2
};

class LD2410Driver {
public:
    enum Response : uint8_t { FAIL = 0, ACK, DATA };

    struct ValuesArray {
        uint8_t values[9] = {0};
        uint8_t N = 0; // max gate index (0..8)
        void setN(uint8_t n) { N = (n <= 8) ? n : 8; }
    };

    struct SensorData {
        uint8_t status = 0xFF;
        uint32_t timestamp = 0; // ms
        uint32_t mTargetDistance = 0;
        uint8_t mTargetSignal = 0;
        uint32_t sTargetDistance = 0;
        uint8_t sTargetSignal = 0;
        uint32_t distance = 0;
        ValuesArray mTargetSignals; // Enhanced mode only
        ValuesArray sTargetSignals; // Enhanced mode only
    };

    LD2410Driver(uart_port_t uart_num, bool debug = false);

    // Controls
    bool begin();
    void end();
    Response check();
    bool configMode(bool enable = true);
    bool enhancedMode(bool enable = true);
    bool requestMAC();
    bool requestFirmware();
    bool requestResolution();
    bool setResolution(bool fine = false);
    bool requestParameters();
    bool setGateParameters(uint8_t gate, uint8_t movingThreshold, uint8_t stationaryThreshold);
    bool setMovingThreshold(uint8_t gate, uint8_t movingThreshold);
    bool setStationaryThreshold(uint8_t gate, uint8_t stationaryThreshold);
    bool setGateParameters(const ValuesArray &moving, const ValuesArray &stationary, uint8_t noOneWindow = 5);
    bool setMaxGate(uint8_t movingGate, uint8_t stationaryGate, uint8_t noOneWindow = 5);
    bool setNoOneWindow(uint8_t noOneWindow);
    bool setMaxMovingGate(uint8_t movingGate);
    bool setMaxStationaryGate(uint8_t stationaryGate);
    bool requestReset();
    bool requestReboot();
    bool requestBTon();
    bool requestBToff();
    bool setBTpassword(const char *passwd);
    bool resetBTpassword();
    bool setBaud(uint8_t baud); // 1..8 enumeration defined by sensor
    bool requestAuxConfig();
    bool autoThresholds(uint8_t timeout_s = 10);
    AutoStatus getAutoStatus();
    bool setAuxControl(LightControl lc, uint8_t light_threshold, OutputControl oc);
    bool resetAuxControl();

    // Status flags
    bool inConfigMode() const { return isConfig; }
    bool inBasicMode() const { return !isEnhanced; }
    bool inEnhancedMode() const { return isEnhanced; }

    // Presence related
    bool presenceDetected();
    bool stationaryTargetDetected();
    bool movingTargetDetected();

    // Getters
    uint8_t getStatus();
    const char *statusString();
    uint32_t stationaryTargetDistance();
    uint8_t stationaryTargetSignal();
    const ValuesArray &getStationarySignals();
    uint32_t movingTargetDistance();
    uint8_t movingTargetSignal();
    const ValuesArray &getMovingSignals();
    uint32_t detectedDistance();
    const uint8_t *getMACArr();
    std::string getMACStr();
    std::string getFirmware();
    uint8_t getFirmwareMajor();
    uint8_t getFirmwareMinor();
    uint32_t getVersion();
    const SensorData &getSensorData();
    uint8_t getResolution();
    const ValuesArray &getMovingThresholds();
    const ValuesArray &getStationaryThresholds();
    uint8_t getRange();
    uint32_t getRange_cm();
    uint8_t getNoOneWindow();
    uint8_t getMaxMovingGate();
    uint8_t getMaxStationaryGate();
    uint8_t getLightLevel();
    LightControl getLightControl();
    uint8_t getLightThreshold();
    OutputControl getOutputControl();
    uint8_t getOutLevel();

private:
    // UART
    uart_port_t uart_num;
    bool debug_mode = false;

    // Internal state
    SensorData sData;
    ValuesArray stationaryThresholds;
    ValuesArray movingThresholds;
    uint8_t maxRange = 0;
    uint8_t noOne_window = 0;
    uint8_t lightLevel = 0;
    uint8_t outLevel = 0;
    uint8_t lightThreshold = 0;
    LightControl lightControl = LightControl::NOT_SET;
    OutputControl outputControl = OutputControl::NOT_SET;
    AutoStatus autoStatus = AutoStatus::NOT_SET;
    uint32_t version = 0;
    uint32_t bufferSize = 0;
    uint8_t MAC[6] = {0};
    std::string MACstr;
    std::string firmwareStr;
    uint8_t firmwareMajor = 0;
    uint8_t firmwareMinor = 0;
    int fineRes = -1; // -1 unknown; 0 coarse 75cm; 1 fine 20cm
    bool isEnhanced = false;
    bool isConfig = false;

    // Buffers
    uint8_t inBuf[LD2410_BUFFER_SIZE];
    uint8_t inBufI = 0;
    uint8_t headBuf[4] = {0};
    uint8_t headBufI = 0;

    // Timing
    uint32_t timeout_ms = 2000; // command timeout
    uint32_t dataLifespan_ms = 500; // validity of last data

    // Helpers
    bool isDataValid() const;
    bool sendCommand(const uint8_t *cmd, size_t explicit_len = 0); // If explicit_len==0 uses (cmd[0]+2)
    bool readFrame();
    bool processAck();
    bool processData();
    uint32_t nowMillis() const; // wrapper around esp_timer
    void debugHex(const uint8_t *buf, size_t len, const char *prefix = nullptr);
    std::string byteToHex(uint8_t b, bool addZero = true) const;
    bool waitForAck(const uint8_t *expectedCmdIds = nullptr, size_t count = 0, uint32_t giveUpAt = 0);
};
