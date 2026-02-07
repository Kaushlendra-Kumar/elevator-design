#ifndef DOMAIN_HPP
#define DOMAIN_HPP

#include "Types.hpp"
#include <vector>
#include <set>
#include <memory>
#include <mutex>
#include <optional>

// ============== Floor ==============

class Floor {
private:
    int floorNumber_;
    bool upButtonPressed_ = false;
    bool downButtonPressed_ = false;

public:
    explicit Floor(int number);

    // Button operations
    void pressUpButton();
    void pressDownButton();
    void clearUpButton();
    void clearDownButton();

    // Queries
    bool isUpPressed() const;
    bool isDownPressed() const;
    int getNumber() const;
};

// ============== Elevator ==============

class Elevator {
private:
    int id_;
    int currentFloor_;
    Direction direction_ = Direction::Idle;
    ElevatorState state_ = ElevatorState::Idle;
    std::set<int> carCalls_;  // Destination floors (sorted, no duplicates)
    int passengerCount_ = 0;
    int capacity_;
    int ticksRemaining_ = 0;  // For timed operations (moving, doors)

    mutable std::mutex mutex_;

public:
    Elevator(int id, int capacity, int startFloor = 1);

    // Getters (thread-safe)
    int getId() const;
    int getCurrentFloor() const;
    Direction getDirection() const;
    ElevatorState getState() const;
    int getPassengerCount() const;
    int getCapacity() const;
    std::set<int> getCarCalls() const;
    int getTicksRemaining() const;

    // Car call management
    void addCarCall(int floor);
    void removeCarCall(int floor);
    bool hasCarCallAt(int floor) const;
    bool hasAnyCarCalls() const;

    // State transitions
    void startMoving(Direction dir, int ticksToArrive);
    void decrementTick();
    void arriveAtFloor(int floor);
    void openDoors(int ticksToOpen);
    void setDoorsOpen(int ticksOpen);
    void closeDoors(int ticksToClose);
    void setIdle();

    // Query helpers
    bool hasCallsAbove() const;
    bool hasCallsBelow() const;
    std::optional<int> getNextCarCallInDirection() const;

    // Cost calculation for scheduling
    int costToServe(int floor, Direction dir, int numFloors) const;

    // Passenger management
    bool canBoard() const;
    void boardPassenger();
    void alightPassenger();
};

// ============== Building ==============

class Building {
private:
    std::vector<Floor> floors_;
    std::vector<std::unique_ptr<Elevator>> elevators_;
    Config config_;
    mutable std::mutex mutex_;

public:
    explicit Building(const Config& config);

    // Accessors
    int getNumFloors() const;
    int getNumElevators() const;
    const Config& getConfig() const;

    // Elevator access
    Elevator& getElevator(int id);
    const Elevator& getElevator(int id) const;

    // Floor access
    Floor& getFloor(int number);
    const Floor& getFloor(int number) const;

    // Hall call management
    void registerHallCall(int floor, Direction dir);
    void clearHallCall(int floor, Direction dir);
    bool hasHallCall(int floor, Direction dir) const;

    // Get all pending hall calls
    std::vector<std::pair<int, Direction>> getAllHallCalls() const;

    // Validation
    bool isValidFloor(int floor) const;
    bool isValidElevator(int id) const;
};

#endif // DOMAIN_HPP
