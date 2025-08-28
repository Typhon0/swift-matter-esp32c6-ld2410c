#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <cstdint>

void ld2410c_init();
void ld2410c_poll();
bool ld2410c_is_present();
uint8_t ld2410c_status(); // returns raw status byte (0=no,1=move,2=still,3=both)

#ifdef __cplusplus
}
#endif