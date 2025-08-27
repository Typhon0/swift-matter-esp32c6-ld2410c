#ifndef BRIDGING_HEADER_H
#define BRIDGING_HEADER_H

#include <stddef.h> // For size_t

// There seems to be an assumption in some C++ headers that strnlen and strdup
// are implicitly available, but this is not the case when importing into Swift.
// We manually declare them here as a workaround.
#ifdef __cplusplus
extern "C" {
#endif
size_t strnlen(const char *s, size_t maxlen);
char *strdup(const char *s1);
#ifdef __cplusplus
}
#endif

// C++ HEADERS
#if __cplusplus
#include <esp_matter.h>
#include <esp_matter_core.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_cluster.h>
#include <esp_matter_attribute.h>
#include <esp_matter_event.h>
#include <app/server/Server.h>
#include <unistd.h>
#include "ld2410c_wrapper.h"
#include "MatterInterface.h"
#endif

// C HEADERS
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include "freertos_utils.h"

#ifdef __cplusplus
}
#endif

#endif /* BRIDGING_HEADER_H */

