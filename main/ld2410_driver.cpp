// New full-featured implementation
#include "ld2410_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <cstring>
#include <sstream>
#include <iomanip>

static const char *TAG = "LD2410";

// Protocol constants (from Arduino library)
static const uint8_t HEAD_DATA[4]  = {0xF4,0xF3,0xF2,0xF1};
static const uint8_t TAIL_DATA[4]  = {0xF8,0xF7,0xF6,0xF5};
static const uint8_t HEAD_CFG[4]   = {0xFD,0xFC,0xFB,0xFA};
static const uint8_t TAIL_CFG[4]   = {0x04,0x03,0x02,0x01};

// Command frames (length (LE) + payload). We store only payload w/out length? We'll compose send.
// For simplicity replicate original arrays including length words.
// Format: first 2 bytes little-endian length of payload following? Original Arduino code: command[0] + 2 total frame length field? We'll mimic: command[0] = payload length L, command bytes = [L,0, <payload L bytes>]
// We'll store full command as Arduino: sizeLow, sizeHigh, CMD, 0, optional extra...

// Helper macros to build short constant arrays
#define CMD_SIMPLE(lenLo,lenHi,cmd,zero) {lenLo,lenHi,cmd,zero}

static const uint8_t CMD_CONFIG_ENABLE[]   = {0x04,0x00,0xFF,0x00,0x01,0x00};
static const uint8_t CMD_CONFIG_DISABLE[]  = {0x02,0x00,0xFE,0x00};
static const uint8_t CMD_QUERY_FIRMWARE[]  = {0x02,0x00,0xA0,0x00};
static const uint8_t CMD_QUERY_MAC[]       = {0x04,0x00,0xA5,0x00,0x01,0x00};
static const uint8_t CMD_QUERY_PARAM[]     = {0x02,0x00,0x61,0x00};
static const uint8_t CMD_ENG_ON[]          = {0x02,0x00,0x62,0x00};
static const uint8_t CMD_ENG_OFF[]         = {0x02,0x00,0x63,0x00};
static const uint8_t CMD_QUERY_RES[]       = {0x02,0x00,0xAB,0x00};
static const uint8_t CMD_RES_COARSE[]      = {0x04,0x00,0xAA,0x00,0x00,0x00};
static const uint8_t CMD_RES_FINE[]        = {0x04,0x00,0xAA,0x00,0x01,0x00};
static const uint8_t CMD_QUERY_AUX[]       = {0x02,0x00,0xAE,0x00};
static const uint8_t CMD_AUX_DEFAULT[]     = {0x06,0x00,0xAD,0x00,0x00,0x80,0x00,0x00};
static const uint8_t CMD_AUTO_BEGIN[]      = {0x04,0x00,0x0B,0x00,0x0A,0x00};
static const uint8_t CMD_AUTO_QUERY[]      = {0x02,0x00,0x1B,0x00};
static const uint8_t CMD_RESET[]           = {0x02,0x00,0xA2,0x00};
static const uint8_t CMD_REBOOT[]          = {0x02,0x00,0xA3,0x00};
static const uint8_t CMD_BT_ON[]           = {0x04,0x00,0xA4,0x00,0x01,0x00};
static const uint8_t CMD_BT_OFF[]          = {0x04,0x00,0xA4,0x00,0x00,0x00};
static const uint8_t CMD_BT_PASSWD[]       = {0x08,0x00,0xA9,0x00,0x48,0x69,0x4C,0x69,0x6E,0x6B}; // default "HiL i n k"

// Gate parameter templates (mutable fields will be patched before sending)
static uint8_t CMD_GATE_PARAM[0x16] = {0x14,0x00,0x64,0x00,0x00,0x00,0x00,0x00,0,0,1,0,0,0,0,0,2,0,0,0,0,0};
static uint8_t CMD_MAX_GATE[0x16]  = {0x14,0x00,0x60,0x00,0x00,0x00,8,0,0,0,1,0,8,0,0,0,2,0,5,0,0,0};

static const char *STATUS_STR[7] = {
    "No target",
    "Moving only",
    "Stationary only",
    "Both moving and stationary",
    "Auto thresholds in progress",
    "Auto thresholds successful",
    "Auto thresholds failed"
};

LD2410Driver::LD2410Driver(uart_port_t uart_num, bool debug) : uart_num(uart_num), debug_mode(debug) {}

bool LD2410Driver::begin() {
    // Attempt to exit config; sensor sometimes boots there
    configMode(false);
    return true; // We'll refine once ack parsing is robust
}

void LD2410Driver::end() {
    isConfig = false;
    isEnhanced = false;
}

uint32_t LD2410Driver::nowMillis() const { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

void LD2410Driver::debugHex(const uint8_t *buf, size_t len, const char *prefix) {
    if (!debug_mode) return;
    if (prefix) ESP_LOGI(TAG, "%s (%d bytes)", prefix, (int)len);
    std::stringstream ss;
    for (size_t i=0;i<len;i++) {
        ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << ' ';
    }
    ESP_LOGI(TAG, "%s", ss.str().c_str());
}

std::string LD2410Driver::byteToHex(uint8_t b, bool addZero) const {
    std::ostringstream ss;
    ss << std::uppercase << std::hex;
    if (addZero && b < 0x10) ss << '0';
    ss << (int)b;
    return ss.str();
}

bool LD2410Driver::sendCommand(const uint8_t *cmd, size_t explicit_len) {
    if (!cmd) return false;
    size_t payloadLen;
    if (explicit_len) {
        payloadLen = explicit_len - 2; // expecting explicit_len includes size bytes? Provide raw payload length logic:
    }
    // Arduino format: first two bytes little endian length of remaining (payload) bytes
    uint16_t lenLE = cmd[0] | (cmd[1] << 8);
    payloadLen = lenLE; // bytes after length words
    size_t totalLen = 2 + payloadLen; // content inside frame

    // Write header
    uart_write_bytes(uart_num, (const char*)HEAD_CFG, sizeof(HEAD_CFG));
    // Write command body
    uart_write_bytes(uart_num, (const char*)cmd, totalLen);
    // Tail
    uart_write_bytes(uart_num, (const char*)TAIL_CFG, sizeof(TAIL_CFG));
    uart_wait_tx_done(uart_num, pdMS_TO_TICKS(50));
    if (debug_mode) debugHex(cmd, totalLen, "Sent CMD");
    return true;
}

bool LD2410Driver::readFrame() {
    // Read size (2 bytes) after header already consumed in processAck/processData.
    return true;
}

bool LD2410Driver::waitForAck(const uint8_t *expectedCmdIds, size_t count, uint32_t giveUpAt) {
    if (!giveUpAt) giveUpAt = nowMillis() + timeout_ms;
    uint8_t last4[4] = {0};
    while (nowMillis() < giveUpAt) {
        uint8_t b;
        int r = uart_read_bytes(uart_num, &b, 1, pdMS_TO_TICKS(15));
        if (r == 1) {
            // shift register
            last4[0] = last4[1];
            last4[1] = last4[2];
            last4[2] = last4[3];
            last4[3] = b;
            // Config ACK frame header
            if (memcmp(last4, HEAD_CFG, 4) == 0) {
                uint8_t lenBytes[2];
                if (uart_read_bytes(uart_num, lenBytes, 2, pdMS_TO_TICKS(40)) != 2) continue;
                uint16_t frameLen = lenBytes[0] | (lenBytes[1] << 8);
                if (frameLen + 4 > LD2410_BUFFER_SIZE) {
                    if (debug_mode) ESP_LOGW(TAG, "ACK frame too large (%u)", (unsigned)frameLen);
                    // drain
                    uint8_t dump[16];
                    while (uart_read_bytes(uart_num, dump, sizeof(dump), 0) > 0) {}
                    continue;
                }
                size_t toRead = frameLen + 4; // payload + tail
                size_t got = uart_read_bytes(uart_num, inBuf, toRead, pdMS_TO_TICKS(120));
                if (got != toRead) continue;
                if (memcmp(inBuf + toRead - 4, TAIL_CFG, 4) != 0) continue;
                // store payload only for parser
                inBufI = frameLen + 4; // including tail for convenience
                if (debug_mode) debugHex(inBuf, inBufI, "ACK payload");
                if (processAck()) return true;
            }
            // Data frame header
            if (memcmp(last4, HEAD_DATA, 4) == 0) {
                // Read until data tail found
                size_t idx = 0;
                uint8_t win[4] = {0};
                uint32_t dataGiveUp = nowMillis() + 80;
                while (nowMillis() < dataGiveUp && idx < LD2410_BUFFER_SIZE) {
                    int n = uart_read_bytes(uart_num, inBuf + idx, 1, pdMS_TO_TICKS(8));
                    if (n == 1) {
                        win[0] = win[1]; win[1] = win[2]; win[2] = win[3]; win[3] = inBuf[idx];
                        idx++;
                        if (memcmp(win, TAIL_DATA, 4) == 0) {
                            inBufI = idx; // payload + tail
                            if (debug_mode) debugHex(inBuf, inBufI, "DATA payload");
                            processData();
                            return true;
                        }
                    }
                }
            }
        } else {
            taskYIELD();
        }
    }
    return false;
}

LD2410Driver::Response LD2410Driver::check() {
    bool got = waitForAck(nullptr, 0, nowMillis() + 5); // short poll
    if (!got) return FAIL;
    if (isDataValid()) return DATA;
    return ACK;
}

bool LD2410Driver::configMode(bool enable) {
    if (enable && isConfig) return true;
    if (!enable && !isConfig) return true;
    sendCommand(enable ? CMD_CONFIG_ENABLE : CMD_CONFIG_DISABLE);
    if (waitForAck(nullptr,0, nowMillis()+500)) {
        // processAck sets flags
        return isConfig == enable;
    }
    return false;
}

bool LD2410Driver::enhancedMode(bool enable) {
    if (isEnhanced == enable) return true;
    bool ok = configMode(true) && sendCommand(enable ? CMD_ENG_ON : CMD_ENG_OFF) && waitForAck(nullptr,0, nowMillis()+500);
    if (ok && !enable) isEnhanced = false;
    if (configMode(false)) return ok;
    return ok;
}

bool LD2410Driver::requestMAC() {
    bool ok = configMode(true) && sendCommand(CMD_QUERY_MAC) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    return ok;
}

bool LD2410Driver::requestFirmware() {
    bool ok = configMode(true) && sendCommand(CMD_QUERY_FIRMWARE) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    return ok;
}

bool LD2410Driver::requestResolution() {
    bool ok = configMode(true) && sendCommand(CMD_QUERY_RES) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    return ok;
}

bool LD2410Driver::setResolution(bool fine) {
    bool ok = configMode(true) && sendCommand(fine ? CMD_RES_FINE : CMD_RES_COARSE) && sendCommand(CMD_QUERY_RES) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    return ok;
}

bool LD2410Driver::requestParameters() {
    bool ok = configMode(true) && sendCommand(CMD_QUERY_PARAM) && waitForAck(nullptr,0, nowMillis()+800);
    configMode(false);
    return ok;
}

bool LD2410Driver::setGateParameters(uint8_t gate, uint8_t movingThreshold, uint8_t stationaryThreshold) {
    if (movingThreshold > 100) movingThreshold = 100;
    if (stationaryThreshold > 100) stationaryThreshold = 100;
    uint8_t *cmd = CMD_GATE_PARAM;
    if (gate > 8) { cmd[6] = 0xFF; cmd[7] = 0xFF; }
    else { cmd[6] = gate; cmd[7] = 0; }
    cmd[12] = movingThreshold;
    cmd[18] = stationaryThreshold;
    bool ok = configMode(true) && sendCommand(cmd) && sendCommand(CMD_QUERY_PARAM) && waitForAck(nullptr,0, nowMillis()+800);
    configMode(false);
    return ok;
}

bool LD2410Driver::setMovingThreshold(uint8_t gate, uint8_t movingThreshold) {
    if (gate > 8) return false;
    if (!movingThresholds.N) requestParameters();
    return setGateParameters(gate, movingThreshold, stationaryThresholds.values[gate]);
}

bool LD2410Driver::setStationaryThreshold(uint8_t gate, uint8_t stationaryThreshold) {
    if (gate > 8) return false;
    if (!stationaryThresholds.N) requestParameters();
    return setGateParameters(gate, movingThresholds.values[gate], stationaryThreshold);
}

bool LD2410Driver::setGateParameters(const ValuesArray &moving, const ValuesArray &stationary, uint8_t noOneWindow) {
    if (!configMode(true)) return false;
    bool success = true;
    for (uint8_t i=0;i<9 && success;i++) {
        success = setGateParameters(i, moving.values[i], stationary.values[i]);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    success = success && setMaxGate(moving.N, stationary.N, noOneWindow);
    configMode(false);
    return success;
}

bool LD2410Driver::setMaxGate(uint8_t movingGate, uint8_t stationaryGate, uint8_t noOneWindow) {
    if (movingGate > 8) movingGate = 8;
    if (stationaryGate > 8) stationaryGate = 8;
    uint8_t *cmd = CMD_MAX_GATE;
    cmd[6]  = movingGate;
    cmd[12] = stationaryGate;
    cmd[18] = noOneWindow;
    bool ok = configMode(true) && sendCommand(cmd) && sendCommand(CMD_QUERY_PARAM) && waitForAck(nullptr,0, nowMillis()+800);
    configMode(false);
    return ok;
}

bool LD2410Driver::setNoOneWindow(uint8_t noOneWindow) {
    if (!maxRange) requestParameters();
    if (noOne_window == noOneWindow) return true;
    return setMaxGate(movingThresholds.N, stationaryThresholds.N, noOneWindow);
}

bool LD2410Driver::setMaxMovingGate(uint8_t movingGate) {
    if (!maxRange) requestParameters();
    if (movingThresholds.N == movingGate) return true;
    if (!noOne_window) noOne_window = 5;
    if (movingGate > 8) movingGate = 8;
    return setMaxGate(movingGate, stationaryThresholds.N, noOne_window);
}

bool LD2410Driver::setMaxStationaryGate(uint8_t stationaryGate) {
    if (!maxRange) requestParameters();
    if (stationaryThresholds.N == stationaryGate) return true;
    if (!noOne_window) noOne_window = 5;
    if (stationaryGate > 8) stationaryGate = 8;
    return setMaxGate(movingThresholds.N, stationaryGate, noOne_window);
}

bool LD2410Driver::requestReset() {
    bool ok = configMode(true) && sendCommand(CMD_RESET) && sendCommand(CMD_QUERY_PARAM) && sendCommand(CMD_QUERY_RES) && waitForAck(nullptr,0, nowMillis()+1000);
    configMode(false);
    return ok;
}

bool LD2410Driver::requestReboot() {
    bool ok = configMode(true) && sendCommand(CMD_REBOOT) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    isEnhanced = false; isConfig = false;
    return ok;
}

bool LD2410Driver::requestBTon() { bool ok = configMode(true) && sendCommand(CMD_BT_ON) && waitForAck(nullptr,0, nowMillis()+500); configMode(false); return ok; }
bool LD2410Driver::requestBToff() { bool ok = configMode(true) && sendCommand(CMD_BT_OFF) && waitForAck(nullptr,0, nowMillis()+500); configMode(false); return ok; }

bool LD2410Driver::setBTpassword(const char *passwd) {
    uint8_t cmd[10]; memcpy(cmd, CMD_BT_PASSWD, 10);
    for (int i=0;i<6;i++) {
        cmd[4+i] = (passwd && (int)strlen(passwd) > i) ? (uint8_t)passwd[i] : (uint8_t)' ';
    }
    bool ok = configMode(true) && sendCommand(cmd) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false); return ok;
}

bool LD2410Driver::resetBTpassword() { return setBTpassword(nullptr); }

bool LD2410Driver::setBaud(uint8_t baud) {
    if (baud < 1 || baud > 8) return false;
    uint8_t cmd[6] = {0x04,0x00,0xA1,0x00,baud,0x00};
    bool ok = configMode(true) && sendCommand(cmd) && requestReboot();
    return ok;
}

bool LD2410Driver::requestAuxConfig() { bool ok = configMode(true) && sendCommand(CMD_QUERY_AUX) && waitForAck(nullptr,0, nowMillis()+500); configMode(false); return ok; }

bool LD2410Driver::autoThresholds(uint8_t timeout_s) {
    uint8_t cmd[6]; memcpy(cmd, CMD_AUTO_BEGIN, 6); cmd[4] = timeout_s; // modify timeout
    bool ok = configMode(true) && sendCommand(cmd) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false); return ok;
}

AutoStatus LD2410Driver::getAutoStatus() {
    bool res = configMode(true) && sendCommand(CMD_AUTO_QUERY) && waitForAck(nullptr,0, nowMillis()+500);
    configMode(false);
    return res ? autoStatus : AutoStatus::NOT_SET;
}

bool LD2410Driver::setAuxControl(LightControl lc, uint8_t light_threshold, OutputControl oc) {
    uint8_t cmd[8]; memcpy(cmd, CMD_AUX_DEFAULT, 8); cmd[4] = (uint8_t)lc; cmd[5] = light_threshold; cmd[6] = (uint8_t)oc;
    bool ok = configMode(true) && sendCommand(cmd) && sendCommand(CMD_QUERY_AUX) && waitForAck(nullptr,0, nowMillis()+500); configMode(false); return ok;
}

bool LD2410Driver::resetAuxControl() { return setAuxControl(LightControl::NO_LIGHT_CONTROL,0, OutputControl::DEFAULT_LOW); }

bool LD2410Driver::isDataValid() const { return (nowMillis() - sData.timestamp) < dataLifespan_ms; }

bool LD2410Driver::presenceDetected() { return isDataValid() && sData.status && sData.status < 4; }
bool LD2410Driver::stationaryTargetDetected() { return isDataValid() && (sData.status == 2 || sData.status == 3); }
bool LD2410Driver::movingTargetDetected() { return isDataValid() && (sData.status == 1 || sData.status == 3); }

uint8_t LD2410Driver::getStatus() { return isDataValid() ? sData.status : 0xFF; }
const char *LD2410Driver::statusString() { return (sData.status < 7) ? STATUS_STR[sData.status] : "Invalid"; }
uint32_t LD2410Driver::stationaryTargetDistance() { return sData.sTargetDistance; }
uint8_t LD2410Driver::stationaryTargetSignal() { return sData.sTargetSignal; }
const LD2410Driver::ValuesArray &LD2410Driver::getStationarySignals() { return sData.sTargetSignals; }
uint32_t LD2410Driver::movingTargetDistance() { return sData.mTargetDistance; }
uint8_t LD2410Driver::movingTargetSignal() { return sData.mTargetSignal; }
const LD2410Driver::ValuesArray &LD2410Driver::getMovingSignals() { return sData.mTargetSignals; }
uint32_t LD2410Driver::detectedDistance() { return sData.distance; }
const uint8_t *LD2410Driver::getMACArr() { if (MACstr.empty()) requestMAC(); return MAC; }
std::string LD2410Driver::getMACStr() { if (MACstr.empty()) requestMAC(); return MACstr; }
std::string LD2410Driver::getFirmware() { if (firmwareStr.empty()) requestFirmware(); return firmwareStr; }
uint8_t LD2410Driver::getFirmwareMajor() { if (!firmwareMajor) requestFirmware(); return firmwareMajor; }
uint8_t LD2410Driver::getFirmwareMinor() { if (!firmwareMajor) requestFirmware(); return firmwareMinor; }
uint32_t LD2410Driver::getVersion() { if (!version) { configMode(true); configMode(false);} return version; }
const LD2410Driver::SensorData &LD2410Driver::getSensorData() { return sData; }
uint8_t LD2410Driver::getResolution() {
    if (fineRes >= 0) return (fineRes == 1) ? 20 : 75;
    requestResolution();
    if (fineRes >= 0) return (fineRes == 1) ? 20 : 75;
    return 0;
}
const LD2410Driver::ValuesArray &LD2410Driver::getMovingThresholds() { if (!maxRange) requestParameters(); return movingThresholds; }
const LD2410Driver::ValuesArray &LD2410Driver::getStationaryThresholds() { if (!maxRange) requestParameters(); return stationaryThresholds; }
uint8_t LD2410Driver::getRange() { if (!maxRange) requestParameters(); return maxRange; }
uint32_t LD2410Driver::getRange_cm() { return (getRange()+1) * getResolution(); }
uint8_t LD2410Driver::getNoOneWindow() { if (!maxRange) requestParameters(); return noOne_window; }
uint8_t LD2410Driver::getMaxMovingGate() { if (!movingThresholds.N) requestParameters(); return movingThresholds.N; }
uint8_t LD2410Driver::getMaxStationaryGate() { if (!stationaryThresholds.N) requestParameters(); return stationaryThresholds.N; }
uint8_t LD2410Driver::getLightLevel() { return lightLevel; }
LightControl LD2410Driver::getLightControl() { if (lightControl == LightControl::NOT_SET) requestAuxConfig(); return lightControl; }
uint8_t LD2410Driver::getLightThreshold() { if (lightControl == LightControl::NOT_SET) requestAuxConfig(); return lightThreshold; }
OutputControl LD2410Driver::getOutputControl() { if (outputControl == OutputControl::NOT_SET) requestAuxConfig(); return outputControl; }
uint8_t LD2410Driver::getOutLevel() { return outLevel; }

bool LD2410Driver::processAck() {
    // inBuf: payload bytes followed by tail (4 bytes). First two bytes = command (little endian)
    if (inBufI < 6) return false; // cmd + status + tail minimal
    if (memcmp(inBuf + inBufI - 4, TAIL_CFG, 4) != 0) return false;
    uint16_t cmdId = inBuf[0] | (inBuf[1] << 8);
    uint16_t status = inBuf[2] | (inBuf[3] << 8);
    if (status) {
        if (debug_mode) ESP_LOGW(TAG, "ACK error for cmd 0x%04X status=0x%04X", cmdId, status);
        return false;
    }
    switch (cmdId) {
        case 0x1FF: // enter config
            isConfig = true;
            version = inBuf[4] | (inBuf[5] << 8);
            bufferSize = inBuf[6] | (inBuf[7] << 8);
            break;
        case 0x1FE: // exit config
            isConfig = false; break;
        case 0x1A5: // MAC
            for (int i=0;i<6;i++) MAC[i] = inBuf[4+i];
            {
                std::ostringstream ss; ss << byteToHex(MAC[0]); for (int i=1;i<6;i++) ss << ":" << byteToHex(MAC[i]); MACstr = ss.str();
            }
            break;
        case 0x1A0: // firmware
            // Layout follows Arduino: bytes after status
            firmwareStr = byteToHex(inBuf[7], false) + std::string(".") + byteToHex(inBuf[6]) + std::string(".") + byteToHex(inBuf[11]) + byteToHex(inBuf[10]) + byteToHex(inBuf[9]) + byteToHex(inBuf[8]);
            firmwareMajor = inBuf[7]; firmwareMinor = inBuf[6];
            break;
        case 0x1AB: // query resolution
            fineRes = inBuf[4];
            break;
        case 0x1AE: // aux config
            lightControl = (LightControl)inBuf[4];
            lightThreshold = inBuf[5];
            outputControl = (OutputControl)inBuf[6];
            break;
        case 0x11B: // auto status
            autoStatus = (AutoStatus)inBuf[4];
            break;
        case 0x1A3: // reboot
            isEnhanced = false; isConfig = false; break;
        case 0x161: // parameters
            maxRange = inBuf[5];
            movingThresholds.setN(inBuf[6]);
            stationaryThresholds.setN(inBuf[7]);
            for (uint8_t i=0;i<=movingThresholds.N;i++) movingThresholds.values[i] = inBuf[8+i];
            for (uint8_t i=0;i<=stationaryThresholds.N;i++) stationaryThresholds.values[i] = inBuf[17+i];
            noOne_window = inBuf[26] | (inBuf[27] << 8);
            break;
        case 0x162: isEnhanced = true; break;
        case 0x163: isEnhanced = false; break;
        default:
            break;
    }
    return true;
}

bool LD2410Driver::processData() {
    // Observed data frame layout (basic mode) from sensor on UART after 0xF4F3F2F1 header:
    // [0] lenLo
    // [1] lenHi  (payload length starting at index 2 up to (1+len))
    // [2] 0x02   (data frame type)
    // [3] 0xAA   (marker)
    // [4] status (bits 0..2 per datasheet: 0 none,1 moving,2 stationary,3 both,4-6 auto states)
    // [5] moving distance LSB
    // [6] moving distance MSB
    // [7] moving signal (0-100)
    // [8] stationary distance LSB
    // [9] stationary distance MSB
    // [10] stationary signal (0-100)
    // [11] detection distance LSB (max of moving/stationary?)
    // [12] detection distance MSB
    // [13] reserved / threshold indicator (often 0x55)
    // [14] reserved (often 0x00)
    // Then 4 byte tail 0xF8 0xF7 0xF6 0xF5 already present at end of buffer.
    // Enhanced mode frames would differ (length & additional per-gate values); not yet observed here.

    if (inBufI < 2 + 13 + 4) return false; // minimal size check
    const uint8_t *p = inBuf;
    uint16_t payloadLen = p[0] | (p[1] << 8);
    // Sanity: payloadLen bytes follow starting at p[2]; we captured payloadLen + 2 + tail (4)
    if (payloadLen + 2 + 4 != inBufI) {
        // Allow some tolerance: if larger (enhanced) we'll add later
        if (payloadLen + 2 + 4 > inBufI) return false;
    }
    if (p[2] != 0x02 || p[3] != 0xAA) return false;

    sData.timestamp = nowMillis();
    sData.status = p[4] & 0x07;
    sData.mTargetDistance = p[5] | (p[6] << 8);
    sData.mTargetSignal = p[7];
    sData.sTargetDistance = p[8] | (p[9] << 8);
    sData.sTargetSignal = p[10];
    sData.distance = p[11] | (p[12] << 8);
    // Basic frame => no per-gate arrays
    isEnhanced = false;
    sData.mTargetSignals.setN(0);
    sData.sTargetSignals.setN(0);
    // Light / out levels not provided in this frame type
    lightLevel = 0;
    outLevel = 0;
    return true;
}
