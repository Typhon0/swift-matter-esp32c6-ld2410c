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
    occupancy_sensor::config_t occupancy_sensor_config;
    occupancy_sensor_config.occupancy_sensing.feature_flags = (1 << 0) | (1 << 1);
    occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type = 4;
    occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap = (1 << 2);

    endpoint_t *endpoint = occupancy_sensor::create(cpp_node, &occupancy_sensor_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!endpoint) {
        return 0;
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

}
