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

// Provided by MatterInterface to bind endpoint and update attributes
void ld2410c_set_vendor_endpoint(uint16_t endpoint_id);
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
void ld2410c_update_vendor_arrays(
	const uint8_t *moving_signals, uint8_t moving_len,
	const uint8_t *stationary_signals, uint8_t stationary_len,
	const uint8_t *moving_thresholds, uint8_t mt_len,
	const uint8_t *stationary_thresholds, uint8_t st_len,
	const char *fw_str
);

#ifdef __cplusplus
}
#endif