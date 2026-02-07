#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include "Types.hpp"
#include "Domain.hpp"
#include "EventQueue.hpp"
#include "Scheduler.hpp"
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>

// ============== Logger ==============

class Logger {
private:
    mutable std::mutex mutex_;
    std::ostream& out_;
    bool enabled_;
    std::atomic<int>* tickRef_ = nullptr;  // Reference to current tick

public:
    explicit Logger(std::ostream& out = std::cout, bool enabled = true);

    void setTickReference(std::atomic<int>* tick);

    void log(const std::string& message);
    void logEvent(const Event& event);
    void logElevatorState(const Elevator& elev);
    void logHallCall(int floor, Direction dir);
    void logCarCall(int elevatorId, int floor);
    void logAssignment(int elevatorId, int floor, Direction dir);

    void enable();
    void disable();
    bool isEnabled() const;

private:
    std::string getTimestamp() const;
};

// ============== Simulation Engine ==============

class SimulationEngine {
private:
    Building building_;
    std::unique_ptr<IScheduler> scheduler_;
    EventQueue<Event> eventQueue_;
    Logger logger_;
    Config config_;

    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    std::atomic<int> currentTick_{0};

public:
    explicit SimulationEngine(const Config& config);
    ~SimulationEngine();

    // Non-copyable
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const;

    // Commands (from CLI or external)
    void requestHallCall(int floor, Direction dir);
    void requestCarCall(int elevatorId, int floor);

    // Status
    void printStatus() const;
    int getCurrentTick() const;
    const Building& getBuilding() const;

    // Access for testing
    Building& getBuildingMutable();
    IScheduler& getScheduler();

private:
    void runSimulationLoop();
    void processEvent(const Event& event);
    void processTick();
    void updateElevators();

    void createScheduler();
};

// ============== CLI Helper ==============

class CLI {
private:
    SimulationEngine& engine_;
    std::atomic<bool> running_{true};

public:
    explicit CLI(SimulationEngine& engine);

    void run();
    void stop();

private:
    void printHelp();
    void processCommand(const std::string& line);
    bool parseHallCall(const std::string& args);
    bool parseCarCall(const std::string& args);
};

#endif // SIMULATION_HPP
