#include "Simulation.hpp"
#include <iomanip>
#include <sstream>

// ============== Logger Implementation ==============

Logger::Logger(std::ostream& out, bool enabled)
    : out_(out), enabled_(enabled) {}

void Logger::setTickReference(std::atomic<int>* tick) {
    tickRef_ = tick;
}

void Logger::log(const std::string& message) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << getTimestamp() << " " << message << "\n";
}

void Logger::logEvent(const Event& event) {
    if (!enabled_) return;
    
    std::ostringstream oss;
    oss << "[EVENT] ";
    
    switch (event.type) {
        case EventType::HallCall:
            oss << "HallCall floor=" << event.floor 
                << " dir=" << directionToString(event.direction);
            break;
        case EventType::CarCall:
            oss << "CarCall elevator=" << event.elevatorId 
                << " floor=" << event.floor;
            break;
        case EventType::ElevatorArrived:
            oss << "ElevatorArrived elevator=" << event.elevatorId 
                << " floor=" << event.floor;
            break;
        case EventType::DoorsOpened:
            oss << "DoorsOpened elevator=" << event.elevatorId;
            break;
        case EventType::DoorsClosed:
            oss << "DoorsClosed elevator=" << event.elevatorId;
            break;
        case EventType::Tick:
            oss << "Tick";
            break;
        case EventType::Shutdown:
            oss << "Shutdown";
            break;
    }
    
    log(oss.str());
}

void Logger::logElevatorState(const Elevator& elev) {
    if (!enabled_) return;
    
    std::ostringstream oss;
    oss << "[ELEVATOR " << elev.getId() << "] "
        << "floor=" << elev.getCurrentFloor() << " "
        << "state=" << stateToString(elev.getState()) << " "
        << "dir=" << directionToString(elev.getDirection()) << " "
        << "passengers=" << elev.getPassengerCount();
    
    auto calls = elev.getCarCalls();
    if (!calls.empty()) {
        oss << " carCalls={";
        bool first = true;
        for (int c : calls) {
            if (!first) oss << ",";
            oss << c;
            first = false;
        }
        oss << "}";
    }
    
    log(oss.str());
}

void Logger::logHallCall(int floor, Direction dir) {
    log("[HALL CALL] floor=" + std::to_string(floor) + 
        " dir=" + directionToString(dir));
}

void Logger::logCarCall(int elevatorId, int floor) {
    log("[CAR CALL] elevator=" + std::to_string(elevatorId) + 
        " floor=" + std::to_string(floor));
}

void Logger::logAssignment(int elevatorId, int floor, Direction dir) {
    log("[ASSIGNMENT] elevator=" + std::to_string(elevatorId) + 
        " -> floor=" + std::to_string(floor) + 
        " dir=" + directionToString(dir));
}

void Logger::enable() { enabled_ = true; }
void Logger::disable() { enabled_ = false; }
bool Logger::isEnabled() const { return enabled_; }

std::string Logger::getTimestamp() const {
    std::ostringstream oss;
    oss << "[";
    if (tickRef_) {
        oss << "T" << std::setw(4) << std::setfill('0') << tickRef_->load();
    } else {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        oss << std::put_time(std::localtime(&time), "%H:%M:%S");
    }
    oss << "]";
    return oss.str();
}

// ============== Simulation Engine Implementation ==============

SimulationEngine::SimulationEngine(const Config& config)
    : building_(config), config_(config) {
    
    logger_.setTickReference(&currentTick_);
    createScheduler();
    
    logger_.log("Simulation initialized with " + 
                std::to_string(config.numFloors) + " floors, " +
                std::to_string(config.numElevators) + " elevators");
    logger_.log("Controller: " + scheduler_->getName());
}

SimulationEngine::~SimulationEngine() {
    stop();
}

void SimulationEngine::createScheduler() {
    scheduler_ = ::createScheduler(config_.controllerType, building_, eventQueue_);
}

void SimulationEngine::start() {
    if (running_.load()) return;
    
    running_.store(true);
    logger_.log("Simulation starting...");
    
    // Start simulation loop thread
    threads_.emplace_back(&SimulationEngine::runSimulationLoop, this);
}

void SimulationEngine::stop() {
    if (!running_.load()) return;
    
    logger_.log("Simulation stopping...");
    running_.store(false);
    
    // Signal shutdown to event queue
    eventQueue_.shutdown();
    
    // Wait for all threads
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
    
    logger_.log("Simulation stopped.");
}

bool SimulationEngine::isRunning() const {
    return running_.load();
}

void SimulationEngine::requestHallCall(int floor, Direction dir) {
    if (!building_.isValidFloor(floor)) {
        logger_.log("[ERROR] Invalid floor: " + std::to_string(floor));
        return;
    }
    
    if (dir == Direction::Idle) {
        logger_.log("[ERROR] Hall call must have Up or Down direction");
        return;
    }
    
    // Boundary checks
    if (floor == 1 && dir == Direction::Down) {
        logger_.log("[WARN] Cannot go down from floor 1");
        return;
    }
    if (floor == building_.getNumFloors() && dir == Direction::Up) {
        logger_.log("[WARN] Cannot go up from top floor");
        return;
    }
    
    logger_.logHallCall(floor, dir);
    
    Event event;
    event.type = EventType::HallCall;
    event.floor = floor;
    event.direction = dir;
    eventQueue_.push(event);
}

void SimulationEngine::requestCarCall(int elevatorId, int floor) {
    if (!building_.isValidElevator(elevatorId)) {
        logger_.log("[ERROR] Invalid elevator: " + std::to_string(elevatorId));
        return;
    }
    if (!building_.isValidFloor(floor)) {
        logger_.log("[ERROR] Invalid floor: " + std::to_string(floor));
        return;
    }
    
    logger_.logCarCall(elevatorId, floor);
    
    Event event;
    event.type = EventType::CarCall;
    event.elevatorId = elevatorId;
    event.floor = floor;
    eventQueue_.push(event);
}

void SimulationEngine::printStatus() const {
    std::cout << "\n========== Status at Tick " << currentTick_.load() << " ==========\n";
    
    // Print elevator states
    for (int i = 0; i < building_.getNumElevators(); ++i) {
        const Elevator& elev = building_.getElevator(i);
        std::cout << "Elevator " << i << ": "
                  << "Floor " << elev.getCurrentFloor() << ", "
                  << stateToString(elev.getState()) << ", "
                  << directionToString(elev.getDirection());
        
        auto calls = elev.getCarCalls();
        if (!calls.empty()) {
            std::cout << ", CarCalls: {";
            bool first = true;
            for (int c : calls) {
                if (!first) std::cout << ", ";
                std::cout << c;
                first = false;
            }
            std::cout << "}";
        }
        std::cout << "\n";
    }
    
    // Print hall calls
    auto hallCalls = building_.getAllHallCalls();
    if (!hallCalls.empty()) {
        std::cout << "Hall Calls: ";
        for (const auto& [floor, dir] : hallCalls) {
            std::cout << floor << directionToString(dir)[0] << " ";
        }
        std::cout << "\n";
    }
    
    std::cout << "==========================================\n\n";
}

int SimulationEngine::getCurrentTick() const {
    return currentTick_.load();
}

const Building& SimulationEngine::getBuilding() const {
    return building_;
}

Building& SimulationEngine::getBuildingMutable() {
    return building_;
}

IScheduler& SimulationEngine::getScheduler() {
    return *scheduler_;
}

void SimulationEngine::runSimulationLoop() {
    while (running_.load()) {
        // Wait for tick duration
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.tickDurationMs)
        );
        
        if (!running_.load()) break;
        
        // Process tick
        processTick();
        ++currentTick_;
        
        // Process any pending events
        while (auto event = eventQueue_.tryPop()) {
            processEvent(*event);
        }
    }
}

void SimulationEngine::processEvent(const Event& event) {
    logger_.logEvent(event);
    
    switch (event.type) {
        case EventType::HallCall:
            scheduler_->handleHallCall(event.floor, event.direction);
            break;
            
        case EventType::CarCall:
            scheduler_->handleCarCall(event.elevatorId, event.floor);
            break;
            
        case EventType::ElevatorArrived:
            scheduler_->onElevatorArrived(event.elevatorId, event.floor);
            break;
            
        case EventType::DoorsOpened:
            scheduler_->onDoorsOpened(event.elevatorId, event.floor);
            break;
            
        case EventType::DoorsClosed:
            scheduler_->onDoorsClosed(event.elevatorId);
            break;
            
        case EventType::Shutdown:
            running_.store(false);
            break;
            
        default:
            break;
    }
}

void SimulationEngine::processTick() {
    updateElevators();
    scheduler_->tick();
}

void SimulationEngine::updateElevators() {
    for (int i = 0; i < building_.getNumElevators(); ++i) {
        Elevator& elev = building_.getElevator(i);
        ElevatorState state = elev.getState();
        
        if (state == ElevatorState::Moving) {
            elev.decrementTick();
            if (elev.getTicksRemaining() == 0) {
                // Arrived at next floor
                int current = elev.getCurrentFloor();
                int next = (elev.getDirection() == Direction::Up) ? current + 1 : current - 1;
                elev.arriveAtFloor(next);
                
                Event event;
                event.type = EventType::ElevatorArrived;
                event.elevatorId = i;
                event.floor = next;
                eventQueue_.push(event);
                
                logger_.logElevatorState(elev);
            }
        }
        else if (state == ElevatorState::DoorsOpening) {
            elev.decrementTick();
            if (elev.getTicksRemaining() == 0) {
                elev.setDoorsOpen(config_.doorOpenTicks);
                
                Event event;
                event.type = EventType::DoorsOpened;
                event.elevatorId = i;
                event.floor = elev.getCurrentFloor();
                eventQueue_.push(event);
            }
        }
        else if (state == ElevatorState::DoorsOpen) {
            elev.decrementTick();
            if (elev.getTicksRemaining() == 0) {
                elev.closeDoors(1);  // 1 tick to close
            }
        }
        else if (state == ElevatorState::DoorsClosing) {
            elev.decrementTick();
            if (elev.getTicksRemaining() == 0) {
                // Check if more work
                if (elev.hasAnyCarCalls() || !building_.getAllHallCalls().empty()) {
                    Event event;
                    event.type = EventType::DoorsClosed;
                    event.elevatorId = i;
                    eventQueue_.push(event);
                } else {
                    elev.setIdle();
                }
            }
        }
    }
}

// ============== CLI Implementation ==============

CLI::CLI(SimulationEngine& engine) : engine_(engine) {}

void CLI::run() {
    printHelp();
    
    std::string line;
    while (running_.load() && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        processCommand(line);
    }
}

void CLI::stop() {
    running_.store(false);
}

void CLI::printHelp() {
    std::cout << "\n=== Elevator Simulation CLI ===\n"
              << "Commands:\n"
              << "  hall <floor> <u|d>  - Hall call (e.g., 'hall 5 u')\n"
              << "  car <elev> <floor>  - Car call (e.g., 'car 0 8')\n"
              << "  status              - Print current status\n"
              << "  help                - Show this help\n"
              << "  quit                - Exit simulation\n"
              << "\n";
}

void CLI::processCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "hall") {
        std::string args;
        std::getline(iss, args);
        if (!parseHallCall(args)) {
            std::cout << "Usage: hall <floor> <u|d>\n";
        }
    }
    else if (cmd == "car") {
        std::string args;
        std::getline(iss, args);
        if (!parseCarCall(args)) {
            std::cout << "Usage: car <elevator_id> <floor>\n";
        }
    }
    else if (cmd == "status") {
        engine_.printStatus();
    }
    else if (cmd == "help") {
        printHelp();
    }
    else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
        engine_.stop();
        running_.store(false);
    }
    else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for usage.\n";
    }
}

bool CLI::parseHallCall(const std::string& args) {
    std::istringstream iss(args);
    int floor;
    char dirChar;
    
    if (!(iss >> floor >> dirChar)) {
        return false;
    }
    
    Direction dir;
    if (dirChar == 'u' || dirChar == 'U') {
        dir = Direction::Up;
    } else if (dirChar == 'd' || dirChar == 'D') {
        dir = Direction::Down;
    } else {
        return false;
    }
    
    engine_.requestHallCall(floor, dir);
    return true;
}

bool CLI::parseCarCall(const std::string& args) {
    std::istringstream iss(args);
    int elevatorId, floor;
    
    if (!(iss >> elevatorId >> floor)) {
        return false;
    }
    
    engine_.requestCarCall(elevatorId, floor);
    return true;
}
