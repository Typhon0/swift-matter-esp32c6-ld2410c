#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

TickType_t ms_to_ticks(uint32_t ms);

#ifdef __cplusplus
}
#endif
