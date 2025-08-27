#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void ld2410c_init();
void ld2410c_poll();
bool ld2410c_is_present();

#ifdef __cplusplus
}
#endif