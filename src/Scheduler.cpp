#include "Scheduler.hpp"
#include <algorithm>
#include <limits>

// ============== Master Controller Implementation ==============

MasterController::MasterController(Building& building, EventQueue<Event>& queue)
    : building_(building), eventQueue_(queue) {}

void MasterController::handleHallCall(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto key = std::make_pair(floor, dir);
    
    // Check if already assigned
    if (assignments_.find(key) != assignments_.end()) {
        return;  // Already assigned
    }
    
    // Register in building
    building_.registerHallCall(floor, dir);
    
    // Select best elevator
    int elevatorId = selectElevator(floor, dir);
    if (elevatorId >= 0) {
        assignments_[key] = elevatorId;
        dispatchElevator(elevatorId);
    }
}

void MasterController::handleCarCall(int elevatorId, int floor) {
    if (!building_.isValidElevator(elevatorId) || !building_.isValidFloor(floor)) {
        return;
    }
    
    building_.getElevator(elevatorId).addCarCall(floor);
    dispatchElevator(elevatorId);
}

void MasterController::onElevatorArrived(int elevatorId, int floor) {
    // Check and clear hall calls served at this floor
    Elevator& elev = building_.getElevator(elevatorId);
    Direction dir = elev.getDirection();
    
    // Clear hall call if this elevator was assigned
    if (auto assignment = getAssignment(floor, dir); assignment && *assignment == elevatorId) {
        clearAssignment(floor, dir);
        building_.clearHallCall(floor, dir);
    }
    
    // Clear car call
    elev.removeCarCall(floor);
}

void MasterController::onDoorsOpened(int elevatorId, int floor) {
    // Passengers board/alight here
    // In a more complete simulation, we'd handle passenger movement
    (void)elevatorId;
    (void)floor;
}

void MasterController::onDoorsClosed(int elevatorId) {
    dispatchElevator(elevatorId);
}

void MasterController::tick() {
    // Process any pending reassignments or idle elevator dispatch
    for (int i = 0; i < building_.getNumElevators(); ++i) {
        Elevator& elev = building_.getElevator(i);
        if (elev.getState() == ElevatorState::Idle) {
            dispatchElevator(i);
        }
    }
}

int MasterController::selectElevator(int floor, Direction dir) {
    int bestElevator = -1;
    int bestCost = std::numeric_limits<int>::max();
    
    for (int i = 0; i < building_.getNumElevators(); ++i) {
        const Elevator& elev = building_.getElevator(i);
        int cost = calculateCost(elev, floor, dir);
        
        if (cost < bestCost) {
            bestCost = cost;
            bestElevator = i;
        }
    }
    
    return bestElevator;
}

int MasterController::calculateCost(const Elevator& elev, int floor, Direction dir) {
    return elev.costToServe(floor, dir, building_.getNumFloors());
}

void MasterController::dispatchElevator(int elevatorId) {
    Elevator& elev = building_.getElevator(elevatorId);
    
    if (elev.getState() != ElevatorState::Idle) {
        return;  // Already busy
    }
    
    // Find next destination: car calls + assigned hall calls
    std::set<int> destinations = elev.getCarCalls();
    
    // Add assigned hall call destinations
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [key, assignedId] : assignments_) {
            if (assignedId == elevatorId) {
                destinations.insert(key.first);
            }
        }
    }
    
    if (destinations.empty()) {
        return;  // Nothing to do
    }
    
    // Find closest destination
    int current = elev.getCurrentFloor();
    auto closest = std::min_element(destinations.begin(), destinations.end(),
        [current](int a, int b) {
            return std::abs(a - current) < std::abs(b - current);
        });
    
    int target = *closest;
    Direction dir = (target > current) ? Direction::Up : Direction::Down;
    
    if (target == current) {
        // Already at destination, open doors
        elev.openDoors(building_.getConfig().doorOpenTicks);
    } else {
        elev.startMoving(dir, building_.getConfig().floorTravelTicks);
    }
}

bool MasterController::shouldStopAtFloor(int elevatorId, int floor) {
    const Elevator& elev = building_.getElevator(elevatorId);
    
    // Check car call
    if (elev.hasCarCallAt(floor)) {
        return true;
    }
    
    // Check hall call assignment
    Direction dir = elev.getDirection();
    if (auto assignment = getAssignment(floor, dir); assignment && *assignment == elevatorId) {
        return true;
    }
    
    return false;
}

std::optional<int> MasterController::getAssignment(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(floor, dir);
    auto it = assignments_.find(key);
    if (it != assignments_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MasterController::clearAssignment(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    assignments_.erase(std::make_pair(floor, dir));
}

// ============== Distributed Controller Implementation ==============

DistributedController::DistributedController(Building& building, EventQueue<Event>& queue)
    : building_(building), eventQueue_(queue) {}

void DistributedController::handleHallCall(int floor, Direction dir) {
    // Register in building and claim board (unclaimed)
    building_.registerHallCall(floor, dir);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = std::make_pair(floor, dir);
        if (claimBoard_.find(key) == claimBoard_.end()) {
            claimBoard_[key] = -1;  // Unclaimed
        }
    }
}

void DistributedController::handleCarCall(int elevatorId, int floor) {
    if (!building_.isValidElevator(elevatorId) || !building_.isValidFloor(floor)) {
        return;
    }
    
    building_.getElevator(elevatorId).addCarCall(floor);
    decideNextAction(elevatorId);
}

void DistributedController::onElevatorArrived(int elevatorId, int floor) {
    Elevator& elev = building_.getElevator(elevatorId);
    Direction dir = elev.getDirection();
    
    // Release claim if we had it
    if (hasClaim(elevatorId, floor, dir)) {
        releaseClaim(floor, dir);
        building_.clearHallCall(floor, dir);
    }
    
    // Clear car call
    elev.removeCarCall(floor);
}

void DistributedController::onDoorsOpened(int elevatorId, int floor) {
    (void)elevatorId;
    (void)floor;
}

void DistributedController::onDoorsClosed(int elevatorId) {
    decideNextAction(elevatorId);
}

void DistributedController::tick() {
    // Each elevator tries to claim calls
    for (int i = 0; i < building_.getNumElevators(); ++i) {
        tryClaimCalls(i);
        
        Elevator& elev = building_.getElevator(i);
        if (elev.getState() == ElevatorState::Idle) {
            decideNextAction(i);
        }
    }
}

void DistributedController::tryClaimCalls(int elevatorId) {
    const Elevator& elev = building_.getElevator(elevatorId);
    
    // Only idle or low-activity elevators should claim new calls
    if (elev.getState() != ElevatorState::Idle && elev.hasAnyCarCalls()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int current = elev.getCurrentFloor();
    int bestFloor = -1;
    Direction bestDir = Direction::Idle;
    int bestDistance = std::numeric_limits<int>::max();
    
    // Find nearest unclaimed call
    for (auto& [key, claimerId] : claimBoard_) {
        if (claimerId == -1) {  // Unclaimed
            int distance = std::abs(key.first - current);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestFloor = key.first;
                bestDir = key.second;
            }
        }
    }
    
    // Claim it
    if (bestFloor >= 0) {
        claimBoard_[std::make_pair(bestFloor, bestDir)] = elevatorId;
    }
}

bool DistributedController::tryClaim(int elevatorId, int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(floor, dir);
    
    auto it = claimBoard_.find(key);
    if (it != claimBoard_.end() && it->second == -1) {
        it->second = elevatorId;
        return true;
    }
    return false;
}

void DistributedController::releaseClaim(int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    claimBoard_.erase(std::make_pair(floor, dir));
}

bool DistributedController::hasClaim(int elevatorId, int floor, Direction dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = std::make_pair(floor, dir);
    auto it = claimBoard_.find(key);
    return it != claimBoard_.end() && it->second == elevatorId;
}

std::vector<std::pair<int, Direction>> DistributedController::getClaimsFor(int elevatorId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<int, Direction>> claims;
    
    for (const auto& [key, claimerId] : claimBoard_) {
        if (claimerId == elevatorId) {
            claims.push_back(key);
        }
    }
    
    return claims;
}

void DistributedController::decideNextAction(int elevatorId) {
    Elevator& elev = building_.getElevator(elevatorId);
    
    if (elev.getState() != ElevatorState::Idle) {
        return;
    }
    
    // Collect destinations: car calls + claimed hall calls
    std::set<int> destinations = elev.getCarCalls();
    
    for (const auto& claim : getClaimsFor(elevatorId)) {
        destinations.insert(claim.first);
    }
    
    if (destinations.empty()) {
        return;
    }
    
    // Go to closest
    int current = elev.getCurrentFloor();
    auto closest = std::min_element(destinations.begin(), destinations.end(),
        [current](int a, int b) {
            return std::abs(a - current) < std::abs(b - current);
        });
    
    int target = *closest;
    
    if (target == current) {
        elev.openDoors(building_.getConfig().doorOpenTicks);
    } else {
        Direction dir = (target > current) ? Direction::Up : Direction::Down;
        elev.startMoving(dir, building_.getConfig().floorTravelTicks);
    }
}

// ============== Factory ==============

std::unique_ptr<IScheduler> createScheduler(
    ControllerType type,
    Building& building,
    EventQueue<Event>& queue
) {
    switch (type) {
        case ControllerType::Master:
            return std::make_unique<MasterController>(building, queue);
        case ControllerType::Distributed:
            return std::make_unique<DistributedController>(building, queue);
    }
    return nullptr;
}
