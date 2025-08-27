#include "freertos_utils.h"
#include "freertos/task.h"

TickType_t ms_to_ticks(uint32_t ms) {
    return pdMS_TO_TICKS(ms);
}
