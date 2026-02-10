#ifndef TYPES_HPP
#define TYPES_HPP

#include <chrono>
#include <string>

// ============== Enums =============

enum class Direction { 
    Up, 
    Down, 
    Idle 
};

enum class ElevatorState {
    Idle,           // Stationary, no pending requests
    Moving,         // Traveling between floors
    DoorsOpening,   // Arrived, opening doors
    DoorsOpen,      // Passengers boarding/alighting
    DoorsClosing    // Preparing to move
};

enum class EventType {
    HallCall,       // Floor button pressed
    CarCall,        // Destination selected in car
    ElevatorArrived,// Elevator reached a floor
    DoorsOpened,
    DoorsClosed,
    Tick,           // Simulation time advance
    Shutdown        // Graceful termination
};

enum class ControllerType { 
    Master, 
    Distributed 
};

// ============== Configuration ==============

struct Config {
    int numFloors = 10;
    int numElevators = 3;
    int carCapacity = 6;
    int tickDurationMs = 500;
    int doorOpenTicks = 3;
    int floorTravelTicks = 2;
    ControllerType controllerType = ControllerType::Master;
};

// ============== Event ==============

struct Event {
    EventType type;
    int floor = -1;
    int elevatorId = -1;
    Direction direction = Direction::Idle;
    std::chrono::steady_clock::time_point timestamp = 
        std::chrono::steady_clock::now();
};

// ============== Utility Functions ==============

inline std::string directionToString(Direction dir) {
    switch (dir) {
        case Direction::Up: return "Up";
        case Direction::Down: return "Down";
        case Direction::Idle: return "Idle";
    }
    return "Unknown";
}

inline std::string stateToString(ElevatorState state) {
    switch (state) {
        case ElevatorState::Idle: return "Idle";
        case ElevatorState::Moving: return "Moving";
        case ElevatorState::DoorsOpening: return "DoorsOpening";
        case ElevatorState::DoorsOpen: return "DoorsOpen";
        case ElevatorState::DoorsClosing: return "DoorsClosing";
    }
    return "Unknown";
}

#endif // TYPES_HPP
