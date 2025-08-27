/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

#define APP_DEVICE_NAME "ESP-Matter-Occupancy-Sensor"
#define APP_DEVICE_TYPE ESP_MATTER_NODE_TYPE_OCCUPANCY_SENSOR

/**
 * @brief Initialize the application driver
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_driver_init();

/**
 * @brief Set the default attribute values for the device
 *
 * @param[in] endpoint_id The endpoint id of the device.
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_driver_set_defaults(uint16_t endpoint_id);
