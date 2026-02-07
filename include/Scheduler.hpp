#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "Types.hpp"
#include "Domain.hpp"
#include "EventQueue.hpp"
#include <map>
#include <mutex>
#include <memory>

// ============== Scheduler Interface ==============

class IScheduler {
public:
    virtual ~IScheduler() = default;

    // Handle incoming requests
    virtual void handleHallCall(int floor, Direction dir) = 0;
    virtual void handleCarCall(int elevatorId, int floor) = 0;

    // Elevator state change notifications
    virtual void onElevatorArrived(int elevatorId, int floor) = 0;
    virtual void onDoorsOpened(int elevatorId, int floor) = 0;
    virtual void onDoorsClosed(int elevatorId) = 0;

    // Called each simulation tick
    virtual void tick() = 0;

    // Get scheduler name for logging
    virtual std::string getName() const = 0;
};

// ============== Master Controller ==============
// Centralized scheduler - makes all assignment decisions

class MasterController : public IScheduler {
private:
    Building& building_;
    EventQueue<Event>& eventQueue_;

    // Assignment tracking: (floor, direction) -> assigned elevator ID
    std::map<std::pair<int, Direction>, int> assignments_;
    mutable std::mutex mutex_;

public:
    MasterController(Building& building, EventQueue<Event>& queue);

    void handleHallCall(int floor, Direction dir) override;
    void handleCarCall(int elevatorId, int floor) override;
    void onElevatorArrived(int elevatorId, int floor) override;
    void onDoorsOpened(int elevatorId, int floor) override;
    void onDoorsClosed(int elevatorId) override;
    void tick() override;
    std::string getName() const override { return "MasterController"; }

private:
    // Find best elevator for a hall call
    int selectElevator(int floor, Direction dir);

    // Calculate cost for elevator to serve a request
    int calculateCost(const Elevator& elev, int floor, Direction dir);

    // Determine and dispatch next action for an elevator
    void dispatchElevator(int elevatorId);

    // Check if elevator should stop at current floor
    bool shouldStopAtFloor(int elevatorId, int floor);

    // Get assignment for a floor/direction
    std::optional<int> getAssignment(int floor, Direction dir);

    // Clear assignment when served
    void clearAssignment(int floor, Direction dir);
};

// ============== Distributed Controller ==============
// Per-car decision making with claim board coordination

class DistributedController : public IScheduler {
private:
    Building& building_;
    EventQueue<Event>& eventQueue_;

    // Claim board: (floor, direction) -> claiming elevator ID (-1 if unclaimed)
    std::map<std::pair<int, Direction>, int> claimBoard_;
    mutable std::mutex mutex_;

public:
    DistributedController(Building& building, EventQueue<Event>& queue);

    void handleHallCall(int floor, Direction dir) override;
    void handleCarCall(int elevatorId, int floor) override;
    void onElevatorArrived(int elevatorId, int floor) override;
    void onDoorsOpened(int elevatorId, int floor) override;
    void onDoorsClosed(int elevatorId) override;
    void tick() override;
    std::string getName() const override { return "DistributedController"; }

private:
    // Each elevator tries to claim unclaimed calls
    void tryClaimCalls(int elevatorId);

    // Claim a specific hall call for an elevator
    bool tryClaim(int elevatorId, int floor, Direction dir);

    // Release claim when served
    void releaseClaim(int floor, Direction dir);

    // Check if this elevator has the claim
    bool hasClaim(int elevatorId, int floor, Direction dir);

    // Get all claims for an elevator
    std::vector<std::pair<int, Direction>> getClaimsFor(int elevatorId);

    // Determine next action for an elevator (distributed decision)
    void decideNextAction(int elevatorId);
};

// ============== Factory ==============

std::unique_ptr<IScheduler> createScheduler(
    ControllerType type,
    Building& building,
    EventQueue<Event>& queue
);

#endif // SCHEDULER_HPP
