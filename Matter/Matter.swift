struct Matter {
    static func start(
        _ deviceEventCallback: @escaping @convention(c) (UnsafeRawPointer?, Int) -> Void
    ) {
        esp_matter_start_wrapper(deviceEventCallback)
    }
}