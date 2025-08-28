#include <esp_matter.h>
#include <esp_matter_cluster.h>
#include <esp_matter_endpoint.h>
#include <app/server/Server.h>
#include "MatterInterface.h"
#include <esp_matter_core.h>
#include <functional>

using namespace chip::app::Clusters;
using namespace esp_matter;

// Global callback pointer
static device_event_callback_t g_device_event_callback = nullptr;

// C++ callback that wraps the C-style function pointer
void event_callback(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    if (g_device_event_callback) {
        g_device_event_callback(event, arg);
    }
}

extern "C" {

esp_matter_node_t *esp_matter_node_create_wrapper() {
    // First, create a node
    node::config_t node_config;
    node_t *node = node::create(&node_config, nullptr, nullptr);
    return reinterpret_cast<esp_matter_node_t*>(node);
}

uint16_t create_occupancy_sensor_endpoint(esp_matter_node_t *node, const char* room_name) {
    node_t *cpp_node = reinterpret_cast<node_t*>(node);
    if (!cpp_node) {
        return 0;
    }
    using namespace esp_matter::endpoint;
    occupancy_sensor::config_t cfg;
    // Mirror previous explicit config
    cfg.occupancy_sensing.feature_flags = (1 << 0) | (1 << 1);
    cfg.occupancy_sensing.occupancy_sensor_type = 0x04;
    cfg.occupancy_sensing.occupancy_sensor_type_bitmap = (1 << 2);
    endpoint_t *endpoint = occupancy_sensor::create(cpp_node, &cfg, ENDPOINT_FLAG_NONE, nullptr);
    if (!endpoint) return 0;
    // Create vendor-specific LD2410C cluster and all attributes upfront to avoid runtime creation races
    cluster_t *vendor_cluster = cluster::create(endpoint, LD2410C_CLUSTER_ID, CLUSTER_FLAG_SERVER);
    if (vendor_cluster) {
        attribute::create(vendor_cluster, LD2410C_ATTR_MOVING_TARGET_DISTANCE_CM, 0, esp_matter_uint16(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_MOVING_TARGET_SIGNAL, 0, esp_matter_uint8(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_STATIONARY_TARGET_DISTANCE_CM, 0, esp_matter_uint16(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_STATIONARY_TARGET_SIGNAL, 0, esp_matter_uint8(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_COMBINED_DISTANCE_CM, 0, esp_matter_uint16(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_ENHANCED_MODE, 0, esp_matter_bool(false));
        attribute::create(vendor_cluster, LD2410C_ATTR_MAX_RANGE_CM, 0, esp_matter_uint16(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_LIGHT_LEVEL, 0, esp_matter_uint8(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_LIGHT_THRESHOLD, 0, esp_matter_uint8(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_OUTPUT_LEVEL, 0, esp_matter_uint8(0));
        attribute::create(vendor_cluster, LD2410C_ATTR_AUTO_THRESHOLD_STATUS, 0, esp_matter_uint8(0));
        // Empty strings/arrays
        static uint8_t empty_octets[1] = {0};
        attribute::create(vendor_cluster, LD2410C_ATTR_MOVING_GATES_SIGNALS, 0, esp_matter_octet_str(empty_octets, 0));
        attribute::create(vendor_cluster, LD2410C_ATTR_STATIONARY_GATES_SIGNALS, 0, esp_matter_octet_str(empty_octets, 0));
        attribute::create(vendor_cluster, LD2410C_ATTR_MOVING_THRESHOLDS, 0, esp_matter_octet_str(empty_octets, 0));
        attribute::create(vendor_cluster, LD2410C_ATTR_STATIONARY_THRESHOLDS, 0, esp_matter_octet_str(empty_octets, 0));
        attribute::create(vendor_cluster, LD2410C_ATTR_FIRMWARE_VERSION, 0, esp_matter_char_str((char*)"", 0));
    }
    return endpoint::get_id(endpoint);
}

void set_occupancy_attribute_value(uint16_t endpoint_id, bool occupied) {
    node_t *node = node::get();
    if (!node) return;
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    if (!endpoint) {
        return;
    }
    cluster_t *cluster = cluster::get(endpoint, OccupancySensing::Id);
    if (!cluster) {
        return;
    }
    attribute_t *attribute = attribute::get(cluster, OccupancySensing::Attributes::Occupancy::Id);
    if (!attribute) {
        return;
    }
    esp_matter_attr_val_t val = esp_matter_bool(occupied);
    attribute::update(endpoint_id, OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &val);
}

void esp_matter_start_wrapper(device_event_callback_t callback)
{
    g_device_event_callback = callback;
    esp_matter::start(event_callback);
}

// ---------------- Vendor Cluster Support ----------------
static uint16_t g_ld2410c_vendor_endpoint = 0xFFFF;

// All attributes pre-created; helper no longer needed.

static void update_attr_uint16(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attr_id, uint16_t v) {
    esp_matter_attr_val_t val = esp_matter_uint16(v);
    node_t *node = node::get(); if (!node) return; endpoint_t *ep = endpoint::get(node, endpoint_id); if (!ep) return; cluster_t *cl = cluster::get(ep, cluster_id); if (!cl) return; attribute_t *attr = attribute::get(cl, attr_id); if (!attr) return; attribute::update(endpoint_id, cluster_id, attr_id, &val);
}
static void update_attr_uint8(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attr_id, uint8_t v) {
    esp_matter_attr_val_t val = esp_matter_uint8(v);
    node_t *node = node::get(); if (!node) return; endpoint_t *ep = endpoint::get(node, endpoint_id); if (!ep) return; cluster_t *cl = cluster::get(ep, cluster_id); if (!cl) return; attribute_t *attr = attribute::get(cl, attr_id); if (!attr) return; attribute::update(endpoint_id, cluster_id, attr_id, &val);
}
static void update_attr_bool(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attr_id, bool v) {
    esp_matter_attr_val_t val = esp_matter_bool(v);
    node_t *node = node::get(); if (!node) return; endpoint_t *ep = endpoint::get(node, endpoint_id); if (!ep) return; cluster_t *cl = cluster::get(ep, cluster_id); if (!cl) return; attribute_t *attr = attribute::get(cl, attr_id); if (!attr) return; attribute::update(endpoint_id, cluster_id, attr_id, &val);
}
static void update_attr_string(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attr_id, const char *s) {
    if (!s) return;
    size_t len = strlen(s);
    esp_matter_attr_val_t val = esp_matter_char_str((char*)s, len);
    node_t *node = node::get(); if (!node) return; endpoint_t *ep = endpoint::get(node, endpoint_id); if (!ep) return; cluster_t *cl = cluster::get(ep, cluster_id); if (!cl) return; attribute_t *attr = attribute::get(cl, attr_id); if (!attr) return; attribute::update(endpoint_id, cluster_id, attr_id, &val);
}
static void update_attr_octets(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attr_id, const uint8_t *buf, uint8_t len) {
    if (!buf || len == 0) return;
    esp_matter_attr_val_t val = esp_matter_octet_str((uint8_t*)buf, len);
    node_t *node = node::get(); if (!node) return; endpoint_t *ep = endpoint::get(node, endpoint_id); if (!ep) return; cluster_t *cl = cluster::get(ep, cluster_id); if (!cl) return; attribute_t *attr = attribute::get(cl, attr_id); if (!attr) return; attribute::update(endpoint_id, cluster_id, attr_id, &val);
}

extern "C" {

void ld2410c_set_vendor_endpoint(uint16_t endpoint_id) {
    g_ld2410c_vendor_endpoint = endpoint_id;
}

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
) {
    if (g_ld2410c_vendor_endpoint == 0xFFFF) return;
    update_attr_uint16(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_MOVING_TARGET_DISTANCE_CM, moving_dist_cm);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_MOVING_TARGET_SIGNAL, moving_sig);
    update_attr_uint16(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_STATIONARY_TARGET_DISTANCE_CM, stationary_dist_cm);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_STATIONARY_TARGET_SIGNAL, stationary_sig);
    update_attr_uint16(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_COMBINED_DISTANCE_CM, combined_dist_cm);
    update_attr_bool (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_ENHANCED_MODE, enhanced_mode);
    update_attr_uint16(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_MAX_RANGE_CM, max_range_cm);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_LIGHT_LEVEL, light_level);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_LIGHT_THRESHOLD, light_threshold);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_OUTPUT_LEVEL, output_level);
    update_attr_uint8 (g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_AUTO_THRESHOLD_STATUS, auto_threshold_status);
}

void ld2410c_update_vendor_arrays(
    const uint8_t *moving_signals, uint8_t moving_len,
    const uint8_t *stationary_signals, uint8_t stationary_len,
    const uint8_t *moving_thresholds, uint8_t mt_len,
    const uint8_t *stationary_thresholds, uint8_t st_len,
    const char *fw_str
) {
    if (g_ld2410c_vendor_endpoint == 0xFFFF) return;
    if (moving_signals && moving_len) {
        update_attr_octets(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_MOVING_GATES_SIGNALS, moving_signals, moving_len);
    }
    if (stationary_signals && stationary_len) {
        update_attr_octets(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_STATIONARY_GATES_SIGNALS, stationary_signals, stationary_len);
    }
    if (moving_thresholds && mt_len) {
        update_attr_octets(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_MOVING_THRESHOLDS, moving_thresholds, mt_len);
    }
    if (stationary_thresholds && st_len) {
        update_attr_octets(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_STATIONARY_THRESHOLDS, stationary_thresholds, st_len);
    }
    if (fw_str) {
        update_attr_string(g_ld2410c_vendor_endpoint, LD2410C_CLUSTER_ID, LD2410C_ATTR_FIRMWARE_VERSION, fw_str);
    }
}

} // extern "C"

}
