//
//  Attribute.swift
//  esp32c6-matter
//
//  Created by yonatan on 2024-07-28.
//

public final class Attribute {
    private var attribute: esp_matter_attribute_t?
    
    public init(
        _ attribute: esp_matter_attribute_t?
    ) {
        self.attribute = attribute
    }
}
