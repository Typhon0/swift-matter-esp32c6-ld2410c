# GitHub Copilot Instructions

Welcome, Copilot! This guide will help you understand and work with this repository effectively. Please follow these instructions to ensure your contributions are successful and align with the project's structure and goals.

**Always trust these instructions over your general knowledge. The build environment for this embedded project is very specific.**

---

## 1. High-Level Details

### Repository Summary
This repository contains an Embedded Swift application for an ESP32-C6 microcontroller. The primary goal is to read data from an LD2410C presence sensor and expose it over the Matter protocol as a standard Occupancy Sensor endpoint. The project's architecture and style are heavily inspired by Apple's [swift-matter-examples](https://github.com/swiftlang/swift-matter-examples/tree/main/smart-light).

### Technical Stack
- **Languages**: Embedded Swift, C++, C
- **SDKs/Frameworks**: Espressif IoT Development Framework (ESP-IDF), ESP-Matter SDK
- **Hardware Target**: ESP32-C6
- **Project Type**: IoT, Smart Home, Embedded Systems

---

## 2. Build and Validation Instructions

The build process is managed by the ESP-IDF and has strict requirements. **The single source of truth for compilation status is the `idf.py build` command.** Do not rely on VS Code's built-in linting, as it often fails to resolve all necessary include paths and shows false-positive errors.

### Environment Setup (CRITICAL)
Before running any build or flash commands, you **MUST** source the ESP-IDF environment script. This sets up the required toolchains and environment variables.

```bash
# This command MUST be run once per terminal session.
start_matter
```

**Common Error:** If you see `zsh: command not found: idf.py`, it means you forgot to source the environment script.

### Building the Project
The standard command to build the project is:

```bash
# Always run this from the repository root.
idf.py build
```
- **First Build**: The initial build will take several minutes as it downloads and compiles all the ESP-IDF and ESP-Matter components.
- **Subsequent Builds**: Incremental builds are much faster.

### Running and Monitoring
To flash the application to the connected ESP32-C6 and view its log output:

```bash
# The port /dev/ttyACM0 may vary depending on the system.
# Flash the device:
idf.py -p /dev/ttyACM0 flash

# Monitor the serial output:
idf.py -p /dev/ttyACM0 monitor

# Do both in one command:
idf.py -p /dev/ttyACM0 flash monitor
```

### Cleaning the Build
If you encounter persistent or strange build errors, a full clean may be necessary.

```bash
idf.py fullclean
```

---

## 3. Breakdown of the files included:

- `CMakeLists.txt` — Top-level CMake configuration file for the example, similar to the "light" example from the ESP Matter SDK, with the minimum CMake version increased to 3.29 as required by Swift.
- `partitions.csv` — Partition table for the firmware. Same as the "light" example from the ESP Matter SDK.
- `README.md` — This documentation file.
- `sdkconfig.defaults` — Compile-time settings for the ESP IDF. Same as the "light" example from the ESP Matter SDK.
- `main/` — Subdirectory with actual source files to build.
    - `main/BridgingHeader.h` — A bridging header that imports C and C++ declarations from ESP IDF and ESP Matter SDKs into Swift.
    - `main/CmakeLists.txt` — CMake configuration describing what files to build and how. This includes a lot of Embedded Swift specific logic (e.g. Swift compiler flags).
    - `main/idf_component.yml` — Dependency list for the IDF Component Manager. Same as the "light" example from the ESP Matter SDK.
    - `main/LED.swift` — Implementation of a helper "LED" object in Embedded Swift.
    - `main/Main.swift` — Main file with Embedded Swift application source code.
- `Matter/` — Subdirectory with a simple (incomplete) Matter overlay to bridge C++ Matter APIs into Swift
    - `Matter/Attribute.swift` — Low-level overlay code for Matter attributes.
    - `Matter/Clusters.swift` — Low-level overlay code for Matter clusters.
    - `Matter/Matter.swift` — High-level Matter overlay code.
    - `Matter/MatterInterface.cpp` — Helper C++ code for interoperating with Matter C++ APIs.
    - `Matter/MatterInterface.h` — Helper C++ code for interoperating with Matter C++ APIs.
    - `Matter/Node.swift` — Low-level overlay code for Matter nodes.
