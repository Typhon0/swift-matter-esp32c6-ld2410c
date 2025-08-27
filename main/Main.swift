@_cdecl("app_main")
func app_main() {
    print("Starting Matter Occupancy Sensor...")

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

    // Create the occupancy sensor device
    let occupancySensor = OccupancySensor(roomName: "Living Room")

    // Start Matter
    Matter.start(deviceEventCallback)

    // Keep the main thread alive
    while true {
        // Simulate occupancy changes
        sleep(5)
        occupancySensor.setOccupied(true)
        print("Occupancy detected")
        sleep(5)
        occupancySensor.setOccupied(false)
        print("Occupancy cleared")
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



