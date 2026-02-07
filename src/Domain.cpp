#include "Domain.hpp"
#include <algorithm>
#include <stdexcept>

// ============== Floor Implementation ==============

Floor::Floor(int number) : floorNumber_(number) {}

void Floor::pressUpButton() { upButtonPressed_ = true; }
void Floor::pressDownButton() { downButtonPressed_ = true; }
void Floor::clearUpButton() { upButtonPressed_ = false; }
void Floor::clearDownButton() { downButtonPressed_ = false; }

bool Floor::isUpPressed() const { return upButtonPressed_; }
bool Floor::isDownPressed() const { return downButtonPressed_; }
int Floor::getNumber() const { return floorNumber_; }

// ============== Elevator Implementation ==============

Elevator::Elevator(int id, int capacity, int startFloor)
    : id_(id), currentFloor_(startFloor), capacity_(capacity) {}

int Elevator::getId() const { 
    std::lock_guard<std::mutex> lock(mutex_);
    return id_; 
}

int Elevator::getCurrentFloor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentFloor_;
}

Direction Elevator::getDirection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return direction_;
}

ElevatorState Elevator::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

int Elevator::getPassengerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return passengerCount_;
}

int Elevator::getCapacity() const {
    return capacity_;  // Immutable, no lock needed
}

std::set<int> Elevator::getCarCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return carCalls_;
}

int Elevator::getTicksRemaining() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ticksRemaining_;
}

void Elevator::addCarCall(int floor) {
    std::lock_guard<std::mutex> lock(mutex_);
    carCalls_.insert(floor);
}

void Elevator::removeCarCall(int floor) {
    std::lock_guard<std::mutex> lock(mutex_);
    carCalls_.erase(floor);
}

bool Elevator::hasCarCallAt(int floor) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return carCalls_.count(floor) > 0;
}

bool Elevator::hasAnyCarCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !carCalls_.empty();
}

void Elevator::startMoving(Direction dir, int ticksToArrive) {
    std::lock_guard<std::mutex> lock(mutex_);
    direction_ = dir;
    state_ = ElevatorState::Moving;
    ticksRemaining_ = ticksToArrive;
}

void Elevator::decrementTick() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ticksRemaining_ > 0) {
        --ticksRemaining_;
    }
}

void Elevator::arriveAtFloor(int floor) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentFloor_ = floor;
    state_ = ElevatorState::DoorsOpening;
}

void Elevator::openDoors(int ticksToOpen) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ElevatorState::DoorsOpening;
    ticksRemaining_ = ticksToOpen;
}

void Elevator::setDoorsOpen(int ticksOpen) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ElevatorState::DoorsOpen;
    ticksRemaining_ = ticksOpen;
}

void Elevator::closeDoors(int ticksToClose) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ElevatorState::DoorsClosing;
    ticksRemaining_ = ticksToClose;
}

void Elevator::setIdle() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ElevatorState::Idle;
    direction_ = Direction::Idle;
    ticksRemaining_ = 0;
}

bool Elevator::hasCallsAbove() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(carCalls_.begin(), carCalls_.end(),
        [this](int f) { return f > currentFloor_; });
}

bool Elevator::hasCallsBelow() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(carCalls_.begin(), carCalls_.end(),
        [this](int f) { return f < currentFloor_; });
}

std::optional<int> Elevator::getNextCarCallInDirection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (carCalls_.empty()) return std::nullopt;
    
    if (direction_ == Direction::Up) {
        auto it = carCalls_.upper_bound(currentFloor_);
        if (it != carCalls_.end()) return *it;
    } else if (direction_ == Direction::Down) {
        auto it = carCalls_.lower_bound(currentFloor_);
        if (it != carCalls_.begin()) return *std::prev(it);
    }
    
    // No calls in current direction, return closest
    return *std::min_element(carCalls_.begin(), carCalls_.end(),
        [this](int a, int b) {
            return std::abs(a - currentFloor_) < std::abs(b - currentFloor_);
        });
}

int Elevator::costToServe(int floor, Direction dir, int numFloors) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int distance = std::abs(currentFloor_ - floor);
    
    if (state_ == ElevatorState::Idle) {
        return distance;
    }
    
    bool sameDirection = (direction_ == dir);
    bool onTheWay = (direction_ == Direction::Up && floor > currentFloor_) ||
                    (direction_ == Direction::Down && floor < currentFloor_);
    
    if (sameDirection && onTheWay) {
        return distance;
    }
    
    // Penalty for opposite direction
    return distance + 2 * numFloors;
}

bool Elevator::canBoard() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return passengerCount_ < capacity_;
}

void Elevator::boardPassenger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (passengerCount_ < capacity_) {
        ++passengerCount_;
    }
}

void Elevator::alightPassenger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (passengerCount_ > 0) {
        --passengerCount_;
    }
}

// ============== Building Implementation ==============

Building::Building(const Config& config) : config_(config) {
    // Create floors (1-indexed)
    floors_.reserve(config.numFloors);
    for (int i = 1; i <= config.numFloors; ++i) {
        floors_.emplace_back(i);
    }
    
    // Create elevators
    elevators_.reserve(config.numElevators);
    for (int i = 0; i < config.numElevators; ++i) {
        elevators_.push_back(
            std::make_unique<Elevator>(i, config.carCapacity, 1)
        );
    }
}

int Building::getNumFloors() const { return config_.numFloors; }
int Building::getNumElevators() const { return config_.numElevators; }
const Config& Building::getConfig() const { return config_; }

Elevator& Building::getElevator(int id) {
    if (!isValidElevator(id)) {
        throw std::out_of_range("Invalid elevator ID: " + std::to_string(id));
    }
    return *elevators_[id];
}

const Elevator& Building::getElevator(int id) const {
    if (!isValidElevator(id)) {
        throw std::out_of_range("Invalid elevator ID: " + std::to_string(id));
    }
    return *elevators_[id];
}

Floor& Building::getFloor(int number) {
    if (!isValidFloor(number)) {
        throw std::out_of_range("Invalid floor number: " + std::to_string(number));
    }
    return floors_[number - 1];  // Convert 1-indexed to 0-indexed
}

const Floor& Building::getFloor(int number) const {
    if (!isValidFloor(number)) {
        throw std::out_of_range("Invalid floor number: " + std::to_string(number));
    }
    return floors_[number - 1];
}

void Building::registerHallCall(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isValidFloor(floor)) return;
    
    Floor& f = floors_[floor - 1];
    if (dir == Direction::Up) {
        f.pressUpButton();
    } else if (dir == Direction::Down) {
        f.pressDownButton();
    }
}

void Building::clearHallCall(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isValidFloor(floor)) return;
    
    Floor& f = floors_[floor - 1];
    if (dir == Direction::Up) {
        f.clearUpButton();
    } else if (dir == Direction::Down) {
        f.clearDownButton();
    }
}

bool Building::hasHallCall(int floor, Direction dir) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isValidFloor(floor)) return false;
    
    const Floor& f = floors_[floor - 1];
    if (dir == Direction::Up) {
        return f.isUpPressed();
    } else if (dir == Direction::Down) {
        return f.isDownPressed();
    }
    return false;
}

std::vector<std::pair<int, Direction>> Building::getAllHallCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<int, Direction>> calls;
    
    for (const auto& floor : floors_) {
        if (floor.isUpPressed()) {
            calls.emplace_back(floor.getNumber(), Direction::Up);
        }
        if (floor.isDownPressed()) {
            calls.emplace_back(floor.getNumber(), Direction::Down);
        }
    }
    
    return calls;
}

bool Building::isValidFloor(int floor) const {
    return floor >= 1 && floor <= config_.numFloors;
}

bool Building::isValidElevator(int id) const {
    return id >= 0 && id < config_.numElevators;
}
