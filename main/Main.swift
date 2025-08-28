@_cdecl("app_main")
func app_main() {
    print("Starting Matter Occupancy Sensor...")

    // Set log levels to reduce verbosity
    esp_log_level_set("*", ESP_LOG_INFO)
    esp_log_level_set("esp_matter", ESP_LOG_DEBUG)
    esp_log_level_set("ld2410c_wrapper", ESP_LOG_INFO)
    // Keep LD2410 at INFO so we can see frame/debug output if enabled in driver
    esp_log_level_set("LD2410", ESP_LOG_INFO)
    esp_log_level_set("esp_matter_attribute", ESP_LOG_INFO)
    esp_log_level_set("wifi", ESP_LOG_WARN)
    esp_log_level_set("chip", ESP_LOG_WARN)


    // Initialize NVS
    let ret = nvs_flash_init()
    if ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND {
        print("NVS flash initialization failed, erasing and retrying...")
        guard nvs_flash_erase() == ESP_OK else {
            print("Failed to erase NVS")
            return
        }
        guard nvs_flash_init() == ESP_OK else {
            print("Failed to initialize NVS after erase")
            return
        }
    }

    // Initialize LD2410C sensor
    ld2410c_init()
    print("LD2410C Initialized")

    // Create the occupancy sensor device
    let occupancySensor = OccupancySensor(roomName: "Living Room")

    // Start Matter
    Matter.start(deviceEventCallback)

    var lastPresence = false

    // Keep the main thread alive
    while true {
        ld2410c_poll()
        let isPresent = ld2410c_is_present()

        if isPresent != lastPresence {
            occupancySensor.setOccupied(isPresent)
            if isPresent {
                print("Occupancy detected")
            } else {
                print("Occupancy cleared")
            }
            lastPresence = isPresent
        }

        // Poll every 250ms
        vTaskDelay(ms_to_ticks(250))
    }
}

func deviceEventCallback(_ event: UnsafeRawPointer?, _ context: Int) {
    // This is a C-style callback, so we need to be careful with types.
    // The event is a pointer to a ChipDeviceEvent, but we can't directly use that type here.
    // We can, however, check the event type ID.
    // The event type for post-attribute-update is device-specific, so we can't rely on a fixed value.
    // For now, we'll just print a generic message.
    print("Received a device event.")
}



