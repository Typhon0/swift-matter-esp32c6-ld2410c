#include "app_priv.h"
#include <esp_matter_cluster.h>
#include <esp_matter_core.h>
#include <esp_matter.h>
#include "MatterInterface.h"

using namespace esp_matter;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;


extern "C" {

uint16_t create_occupancy_sensor_endpoint(esp_matter_node_t *node, const char* room_name) {
    esp_matter::node_t *cpp_node = reinterpret_cast<esp_matter::node_t*>(node);
    
    esp_matter::device_type_t *device_type = esp_matter::endpoint::create_device_type(0x0107);

    esp_matter::endpoint_t* endpoint = esp_matter::endpoint::create(cpp_node, 0, device_type);
    if (!endpoint) {
        return 0;
    }

    esp_matter::cluster::descriptor::create(endpoint, nullptr, esp_matter::CLUSTER_FLAG_SERVER);
    esp_matter::cluster::identify::create(endpoint, nullptr, esp_matter::CLUSTER_FLAG_SERVER);
    
    esp_matter::cluster::fixed_label::config_t label_config;
    label_config.label_list.push_back(std::pair<std::string, std::string>("room", room_name));
    esp_matter::cluster::fixed_label::create(endpoint, &label_config, esp_matter::CLUSTER_FLAG_SERVER);

    esp_matter::cluster::occupancy_sensing::config_t occupancy_config;
    occupancy_config.occupancy = 0;
    occupancy_config.occupancy_sensor_type = 0;
    esp_matter::cluster::occupancy_sensing::create(endpoint, &occupancy_config, esp_matter::CLUSTER_FLAG_SERVER);

    return esp_matter::endpoint::get_id(endpoint);
}

void set_attribute_bool_value_wrapper(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, bool value) {
    esp_matter::attribute::val_t val = esp_matter::attribute::create(attribute_id, value);
    esp_matter::attribute::update(endpoint_id, cluster_id, attribute_id, val);
}

}
