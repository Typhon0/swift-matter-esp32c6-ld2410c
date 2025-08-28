public class OccupancySensor: Device {
    public var endpointId: UInt16
    private var occupied: Bool = false

    public init(roomName: String) {
        let node = Node()
        self.endpointId = create_occupancy_sensor_endpoint(node?.getRaw(), roomName)
    // Register endpoint for vendor LD2410C telemetry updates
    ld2410c_set_vendor_endpoint(self.endpointId)
        super.init(node: node)
    }

    public func setOccupied(_ occupied: Bool) {
        self.occupied = occupied
        set_occupancy_attribute_value(endpointId, occupied)
    }
}

        