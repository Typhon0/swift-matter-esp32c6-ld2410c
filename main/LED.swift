import SwiftIO
import MadMachine

// This file is no longer needed and can be removed.
// The LED logic will be handled by the main application logic.
final class LED {
    private let pin: DigitalOut
    private var isOn: Bool

    init(pin: DigitalOut) {
        self.pin = pin
        self.isOn = false
        off()
    }

    func on() {
        pin.write(true)
        isOn = true
    }

    func off() {
        pin.write(false)
        isOn = false
    }

    func toggle() {
        if isOn {
            off()
        } else {
            on()
        }
    }
}
