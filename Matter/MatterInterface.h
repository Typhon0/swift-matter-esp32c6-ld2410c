#ifndef MATTER_INTERFACE_H
#define MATTER_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for the opaque pointer
typedef struct esp_matter_node_s esp_matter_node_t;

// Forward declaration for event callback
typedef void (*device_event_callback_t)(const void *event, intptr_t arg);

uint16_t create_occupancy_sensor_endpoint(esp_matter_node_t *node, const char *room_name);
void set_occupancy_attribute_value(uint16_t endpoint_id, bool occupied);
esp_matter_node_t *esp_matter_node_create_wrapper();
void esp_matter_start_wrapper(device_event_callback_t callback);

// Vendor (LD2410C) cluster metadata
#define LD2410C_CLUSTER_ID 0xFC00
// Attribute IDs
#define LD2410C_ATTR_MOVING_TARGET_DISTANCE_CM      0x0001
#define LD2410C_ATTR_MOVING_TARGET_SIGNAL           0x0002
#define LD2410C_ATTR_STATIONARY_TARGET_DISTANCE_CM  0x0003
#define LD2410C_ATTR_STATIONARY_TARGET_SIGNAL       0x0004
#define LD2410C_ATTR_COMBINED_DISTANCE_CM           0x0005
#define LD2410C_ATTR_ENHANCED_MODE                  0x0006
#define LD2410C_ATTR_MAX_RANGE_CM                   0x0007
#define LD2410C_ATTR_FIRMWARE_VERSION               0x0008
#define LD2410C_ATTR_MOVING_GATES_SIGNALS           0x0009 // octet string (N bytes)
#define LD2410C_ATTR_STATIONARY_GATES_SIGNALS       0x000A // octet string
#define LD2410C_ATTR_MOVING_THRESHOLDS              0x000B // octet string
#define LD2410C_ATTR_STATIONARY_THRESHOLDS          0x000C // octet string
#define LD2410C_ATTR_LIGHT_LEVEL                    0x000D
#define LD2410C_ATTR_LIGHT_THRESHOLD                0x000E
#define LD2410C_ATTR_OUTPUT_LEVEL                   0x000F
#define LD2410C_ATTR_AUTO_THRESHOLD_STATUS          0x0010

// Set endpoint id for LD2410C vendor cluster updates (called from Swift after creation)
void ld2410c_set_vendor_endpoint(uint16_t endpoint_id);
// Update core scalar attributes
void ld2410c_update_vendor_scalars(
	uint16_t moving_dist_cm,
	uint8_t moving_sig,
	uint16_t stationary_dist_cm,
	uint8_t stationary_sig,
	uint16_t combined_dist_cm,
	bool enhanced_mode,
	uint16_t max_range_cm,
	uint8_t light_level,
	uint8_t light_threshold,
	uint8_t output_level,
	uint8_t auto_threshold_status
);
// Update arrays (signals & thresholds) and firmware version (string)
void ld2410c_update_vendor_arrays(
	const uint8_t *moving_signals, uint8_t moving_len,
	const uint8_t *stationary_signals, uint8_t stationary_len,
	const uint8_t *moving_thresholds, uint8_t mt_len,
	const uint8_t *stationary_thresholds, uint8_t st_len,
	const char *fw_str
);

// Define event constants for Swift
#define MATTER_EVENT_POST_ATTRIBUTE_UPDATE 10 // Corresponds to ESP_MATTER_EVENT_POST_ATTRIBUTE_UPDATE

#ifdef __cplusplus
}
#endif

#endif // MATTER_INTERFACE_H