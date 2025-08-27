#ifndef MATTEuint16_t create_occupancy_sensor_endpoint(esp_matter_node_t *node, const char *room_name);
void set_occupancy_attribute_value(uint16_t endpoint_id, bool occupied);
esp_matter_node_t *esp_matter_node_create();INTERFACE_H
#define MATTER_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for the opaque pointer
typedef struct esp_matter_node_s esp_matter_node_t;

uint16_t create_occupancy_sensor_endpoint(esp_matter_node_t *node, const char *room_name);
void set_occupancy_attribute_value(uint16_t endpoint_id, bool occupied);

#ifdef __cplusplus
}
#endif

#endif // MATTER_INTERFACE_H