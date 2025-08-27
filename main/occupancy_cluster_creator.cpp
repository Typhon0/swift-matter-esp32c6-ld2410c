#include "esp_matter_core.h"
#include "esp_matter_cluster.h"

extern "C" esp_matter::cluster_t* create_occupancy_sensing_cluster(esp_matter::endpoint_t *endpoint, uint8_t occupancy_sensor_type, uint8_t occupancy_sensor_type_bitmap) {
    esp_matter::cluster::occupancy_sensing::config_t config;
    config.occupancy_sensor_type = occupancy_sensor_type;
    config.occupancy_sensor_type_bitmap = occupancy_sensor_type_bitmap;
    return esp_matter::cluster::occupancy_sensing::create(endpoint, &config, esp_matter::cluster_flags::CLUSTER_FLAG_SERVER);
}
