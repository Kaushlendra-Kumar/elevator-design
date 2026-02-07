# Elevator Simulation System

A realistic elevator simulation for modeling building elevator operations with support for Master Controller and Distributed Controller architectures.

## Features

- **Configurable Building**: 1-12 floors, 1-3 elevators
- **Two Controller Modes**:
  - **Master Controller**: Centralized scheduling with LOOK algorithm
  - **Distributed Controller**: Peer-based coordination with claim board
- **Event-Driven Simulation**: Tick-based time model
- **Thread-Safe Design**: Proper synchronization with mutexes and condition variables
- **Interactive CLI**: Real-time request injection and status monitoring

## Project Structure

```
elevator/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── docs/
│   ├── HLD.md              # High-Level Design
│   └── LLD.md              # Low-Level Design
├── include/
│   ├── Types.hpp           # Enums, constants, Event struct
│   ├── EventQueue.hpp      # Thread-safe queue
│   ├── Domain.hpp          # Elevator, Floor, Building
│   ├── Scheduler.hpp       # IScheduler + Controllers
│   └── Simulation.hpp      # Engine, Logger, CLI
├── src/
│   ├── main.cpp            # Entry point
│   ├── Domain.cpp          # Domain implementations
│   ├── Scheduler.cpp       # Controller implementations
│   └── Simulation.cpp      # Engine implementation
└── tests/
    ├── UnitTests.cpp       # GoogleTest unit tests
    └── StressTests.cpp     # Concurrency & load tests
```

## Prerequisites

- **Compiler**: GCC 9+ or Clang 10+ (C++17 support required)
- **CMake**: Version 3.16 or higher
- **Git**: For fetching GoogleTest

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake git
```

### macOS

```bash
brew install cmake
```

## Building

### Standard Build

```bash
# Navigate to project directory
cd elevator

# Create build directory and configure
cmake -B build -S .

# Build
cmake --build build -j4
```

### Build with Sanitizers

**AddressSanitizer** (memory errors):
```bash
cmake -B build -DENABLE_ASAN=ON
cmake --build build
```

**ThreadSanitizer** (data races):
```bash
cmake -B build -DENABLE_TSAN=ON
cmake --build build
```

## Running

### Basic Usage

```bash
./build/elevator
```

### With Options

```bash
# 12 floors, 3 elevators, distributed controller
./build/elevator -f 12 -e 3 -m distributed

# Fast simulation (100ms ticks)
./build/elevator -t 100
```

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-f, --floors <n>` | Number of floors (1-12) | 10 |
| `-e, --elevators <n>` | Number of elevators (1-3) | 3 |
| `-c, --capacity <n>` | Car capacity (1-10) | 6 |
| `-m, --mode <type>` | Controller: master/distributed | master |
| `-t, --tick <ms>` | Tick duration (100-2000 ms) | 500 |
| `-h, --help` | Show help | - |

### Interactive Commands

Once running, use these CLI commands:

```
hall <floor> <u|d>  - Hall call (e.g., 'hall 5 u' for floor 5 going up)
car <elev> <floor>  - Car call (e.g., 'car 0 8' for elevator 0 to floor 8)
status              - Print current status
help                - Show command help
quit                - Exit simulation
```

## Testing

### Run Unit Tests

```bash
./build/unit_tests
```

### Run Stress Tests

```bash
./build/stress_tests
```

### Run Specific Test

```bash
./build/unit_tests --gtest_filter="ElevatorTest.*"
```

## VSCode Setup

1. Open the `elevator` folder in VSCode
2. Install recommended extensions:
   - C/C++ (ms-vscode.cpptools)
   - CMake Tools (ms-vscode.cmake-tools)
3. Press `Ctrl+Shift+P` → "CMake: Configure"
4. Press `Ctrl+Shift+B` to build
5. Press `F5` to debug

## Architecture Overview

### Threading Model

```
Main Thread
    │
    ├── Simulation Loop Thread (processes ticks)
    │
    └── CLI runs on main thread (blocking input)

Synchronization:
- EventQueue: mutex + condition_variable
- Building/Elevator state: mutex protected
- Atomic flags for running/shutdown
```

### Controller Algorithms

**Master Controller (LOOK Algorithm)**:
1. Hall calls assigned to elevator with lowest "cost"
2. Cost = distance + penalty if wrong direction
3. Elevator continues in direction until no more calls ahead

**Distributed Controller (Claim Board)**:
1. Hall calls posted to shared claim board
2. Each idle elevator claims nearest unclaimed call
3. First-come-first-claim prevents duplicate service

## Troubleshooting

### Build Errors

**"CMake version too old"**: Update CMake to 3.16+
```bash
# Ubuntu
sudo apt install cmake
# Or use pip
pip install cmake --upgrade
```

**"GoogleTest not found"**: CMake will fetch it automatically on first build

### Runtime Issues

**"Deadlock/Hang"**: Run with ThreadSanitizer to detect issues
```bash
cmake -B build -DENABLE_TSAN=ON
cmake --build build
./build/elevator
```

**"Segmentation fault"**: Run with AddressSanitizer
```bash
cmake -B build -DENABLE_ASAN=ON
cmake --build build
./build/elevator
```

## License

MIT License - See LICENSE file
