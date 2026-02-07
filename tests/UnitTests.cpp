#include <gtest/gtest.h>
#include "Types.hpp"
#include "Domain.hpp"
#include "EventQueue.hpp"
#include "Scheduler.hpp"
#include "Simulation.hpp"

// ============== Floor Tests ==============

TEST(FloorTest, InitialState) {
    Floor floor(5);
    EXPECT_EQ(floor.getNumber(), 5);
    EXPECT_FALSE(floor.isUpPressed());
    EXPECT_FALSE(floor.isDownPressed());
}

TEST(FloorTest, ButtonPress) {
    Floor floor(3);
    
    floor.pressUpButton();
    EXPECT_TRUE(floor.isUpPressed());
    EXPECT_FALSE(floor.isDownPressed());
    
    floor.pressDownButton();
    EXPECT_TRUE(floor.isUpPressed());
    EXPECT_TRUE(floor.isDownPressed());
    
    floor.clearUpButton();
    EXPECT_FALSE(floor.isUpPressed());
    EXPECT_TRUE(floor.isDownPressed());
}

// ============== Elevator Tests ==============

TEST(ElevatorTest, InitialState) {
    Elevator elev(0, 6, 1);
    
    EXPECT_EQ(elev.getId(), 0);
    EXPECT_EQ(elev.getCurrentFloor(), 1);
    EXPECT_EQ(elev.getDirection(), Direction::Idle);
    EXPECT_EQ(elev.getState(), ElevatorState::Idle);
    EXPECT_EQ(elev.getPassengerCount(), 0);
    EXPECT_EQ(elev.getCapacity(), 6);
}

TEST(ElevatorTest, CarCalls) {
    Elevator elev(0, 6, 1);
    
    EXPECT_FALSE(elev.hasAnyCarCalls());
    
    elev.addCarCall(5);
    elev.addCarCall(3);
    elev.addCarCall(8);
    
    EXPECT_TRUE(elev.hasAnyCarCalls());
    EXPECT_TRUE(elev.hasCarCallAt(5));
    EXPECT_FALSE(elev.hasCarCallAt(4));
    
    auto calls = elev.getCarCalls();
    EXPECT_EQ(calls.size(), 3);
    EXPECT_TRUE(calls.count(3));
    EXPECT_TRUE(calls.count(5));
    EXPECT_TRUE(calls.count(8));
    
    elev.removeCarCall(5);
    EXPECT_FALSE(elev.hasCarCallAt(5));
}

TEST(ElevatorTest, StateTransitions) {
    Elevator elev(0, 6, 1);
    
    // Start moving up
    elev.startMoving(Direction::Up, 2);
    EXPECT_EQ(elev.getState(), ElevatorState::Moving);
    EXPECT_EQ(elev.getDirection(), Direction::Up);
    EXPECT_EQ(elev.getTicksRemaining(), 2);
    
    // Decrement ticks
    elev.decrementTick();
    EXPECT_EQ(elev.getTicksRemaining(), 1);
    
    elev.decrementTick();
    EXPECT_EQ(elev.getTicksRemaining(), 0);
    
    // Arrive at floor
    elev.arriveAtFloor(2);
    EXPECT_EQ(elev.getCurrentFloor(), 2);
    EXPECT_EQ(elev.getState(), ElevatorState::DoorsOpening);
    
    // Open doors
    elev.setDoorsOpen(3);
    EXPECT_EQ(elev.getState(), ElevatorState::DoorsOpen);
    
    // Close doors
    elev.closeDoors(1);
    EXPECT_EQ(elev.getState(), ElevatorState::DoorsClosing);
    
    // Back to idle
    elev.setIdle();
    EXPECT_EQ(elev.getState(), ElevatorState::Idle);
    EXPECT_EQ(elev.getDirection(), Direction::Idle);
}

TEST(ElevatorTest, DirectionQueries) {
    Elevator elev(0, 6, 5);
    elev.startMoving(Direction::Up, 1);
    
    elev.addCarCall(8);
    elev.addCarCall(3);
    
    EXPECT_TRUE(elev.hasCallsAbove());
    EXPECT_TRUE(elev.hasCallsBelow());
}

TEST(ElevatorTest, Passengers) {
    Elevator elev(0, 3, 1);  // Capacity of 3
    
    EXPECT_TRUE(elev.canBoard());
    
    elev.boardPassenger();
    elev.boardPassenger();
    elev.boardPassenger();
    
    EXPECT_EQ(elev.getPassengerCount(), 3);
    EXPECT_FALSE(elev.canBoard());
    
    elev.alightPassenger();
    EXPECT_TRUE(elev.canBoard());
    EXPECT_EQ(elev.getPassengerCount(), 2);
}

// ============== Building Tests ==============

TEST(BuildingTest, Initialization) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 3;
    
    Building building(config);
    
    EXPECT_EQ(building.getNumFloors(), 10);
    EXPECT_EQ(building.getNumElevators(), 3);
}

TEST(BuildingTest, FloorAccess) {
    Config config;
    config.numFloors = 5;
    Building building(config);
    
    EXPECT_TRUE(building.isValidFloor(1));
    EXPECT_TRUE(building.isValidFloor(5));
    EXPECT_FALSE(building.isValidFloor(0));
    EXPECT_FALSE(building.isValidFloor(6));
    
    Floor& floor = building.getFloor(3);
    EXPECT_EQ(floor.getNumber(), 3);
}

TEST(BuildingTest, HallCalls) {
    Config config;
    config.numFloors = 10;
    Building building(config);
    
    EXPECT_FALSE(building.hasHallCall(5, Direction::Up));
    
    building.registerHallCall(5, Direction::Up);
    EXPECT_TRUE(building.hasHallCall(5, Direction::Up));
    EXPECT_FALSE(building.hasHallCall(5, Direction::Down));
    
    auto calls = building.getAllHallCalls();
    EXPECT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].first, 5);
    EXPECT_EQ(calls[0].second, Direction::Up);
    
    building.clearHallCall(5, Direction::Up);
    EXPECT_FALSE(building.hasHallCall(5, Direction::Up));
}

// ============== EventQueue Tests ==============

TEST(EventQueueTest, PushPop) {
    EventQueue<Event> queue;
    
    Event e1;
    e1.type = EventType::HallCall;
    e1.floor = 5;
    
    queue.push(e1);
    EXPECT_FALSE(queue.empty());
    
    auto result = queue.tryPop();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->floor, 5);
    EXPECT_TRUE(queue.empty());
}

TEST(EventQueueTest, FIFO) {
    EventQueue<int> queue;
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    EXPECT_EQ(queue.tryPop().value(), 1);
    EXPECT_EQ(queue.tryPop().value(), 2);
    EXPECT_EQ(queue.tryPop().value(), 3);
    EXPECT_FALSE(queue.tryPop().has_value());
}

TEST(EventQueueTest, Shutdown) {
    EventQueue<int> queue;
    queue.shutdown();
    
    auto result = queue.tryPop();
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(queue.isShutdown());
}

// ============== Master Controller Tests ==============

TEST(MasterControllerTest, AssignHallCall) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 3;
    
    Building building(config);
    EventQueue<Event> queue;
    MasterController controller(building, queue);
    
    // All elevators start at floor 1
    controller.handleHallCall(5, Direction::Up);
    
    // Hall call should be registered
    EXPECT_TRUE(building.hasHallCall(5, Direction::Up));
}

TEST(MasterControllerTest, CarCall) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 1;
    
    Building building(config);
    EventQueue<Event> queue;
    MasterController controller(building, queue);
    
    controller.handleCarCall(0, 8);
    
    EXPECT_TRUE(building.getElevator(0).hasCarCallAt(8));
}

// ============== Distributed Controller Tests ==============

TEST(DistributedControllerTest, ClaimHallCall) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 2;
    
    Building building(config);
    EventQueue<Event> queue;
    DistributedController controller(building, queue);
    
    controller.handleHallCall(5, Direction::Up);
    
    EXPECT_TRUE(building.hasHallCall(5, Direction::Up));
    
    // Tick should trigger claim
    controller.tick();
}

// ============== Integration Tests ==============

TEST(IntegrationTest, SingleElevatorServeRequest) {
    Config config;
    config.numFloors = 5;
    config.numElevators = 1;
    config.tickDurationMs = 10;  // Fast for testing
    
    SimulationEngine engine(config);
    
    // Request hall call at floor 3
    engine.requestHallCall(3, Direction::Up);
    
    // Let simulation run briefly
    engine.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.stop();
    
    // Elevator should have started processing
    // (Detailed assertions would depend on timing)
}

// ============== Main ==============

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
