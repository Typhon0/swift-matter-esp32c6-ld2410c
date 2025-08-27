public final class Node {
    private var node: OpaquePointer?
    
    public init?() {
        self.node = esp_matter_node_create_wrapper()
        if self.node == nil {
            print("Failed to create Matter node")
            return nil
        }
        print("Matter node created")
    }
    
    public func getRaw() -> OpaquePointer? {
        return self.node
    }
}