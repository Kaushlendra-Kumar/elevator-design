#include <gtest/gtest.h>
#include "Simulation.hpp"
#include <thread>
#include <random>
#include <vector>
#include <atomic>

// ============== High Traffic Test ==============

TEST(StressTest, HighTrafficMaster) {
    Config config;
    config.numFloors = 12;
    config.numElevators = 3;
    config.tickDurationMs = 50;
    config.controllerType = ControllerType::Master;
    
    SimulationEngine engine(config);
    engine.start();
    
    // Generate 50 random hall calls rapidly
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> floorDist(1, 12);
    std::uniform_int_distribution<> dirDist(0, 1);
    
    for (int i = 0; i < 50; ++i) {
        int floor = floorDist(gen);
        Direction dir = (dirDist(gen) == 0) ? Direction::Up : Direction::Down;
        
        // Adjust for boundary floors
        if (floor == 1) dir = Direction::Up;
        if (floor == 12) dir = Direction::Down;
        
        engine.requestHallCall(floor, dir);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Let simulation process
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    engine.stop();
    
    // If we got here without crash or hang, test passes
    SUCCEED();
}

TEST(StressTest, HighTrafficDistributed) {
    Config config;
    config.numFloors = 12;
    config.numElevators = 3;
    config.tickDurationMs = 50;
    config.controllerType = ControllerType::Distributed;
    
    SimulationEngine engine(config);
    engine.start();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> floorDist(1, 12);
    std::uniform_int_distribution<> dirDist(0, 1);
    
    for (int i = 0; i < 50; ++i) {
        int floor = floorDist(gen);
        Direction dir = (dirDist(gen) == 0) ? Direction::Up : Direction::Down;
        
        if (floor == 1) dir = Direction::Up;
        if (floor == 12) dir = Direction::Down;
        
        engine.requestHallCall(floor, dir);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    engine.stop();
    
    SUCCEED();
}

// ============== Concurrent Access Test ==============

TEST(StressTest, ConcurrentRequests) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 3;
    config.tickDurationMs = 50;
    
    SimulationEngine engine(config);
    engine.start();
    
    std::atomic<int> requestsMade{0};
    const int numThreads = 4;
    const int requestsPerThread = 25;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&engine, &requestsMade, requestsPerThread, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> floorDist(1, 10);
            std::uniform_int_distribution<> elevDist(0, 2);
            std::uniform_int_distribution<> typeDist(0, 1);
            
            for (int i = 0; i < requestsPerThread; ++i) {
                int floor = floorDist(gen);
                
                if (typeDist(gen) == 0) {
                    // Hall call
                    Direction dir = (floor < 5) ? Direction::Up : Direction::Down;
                    if (floor == 1) dir = Direction::Up;
                    if (floor == 10) dir = Direction::Down;
                    engine.requestHallCall(floor, dir);
                } else {
                    // Car call
                    int elevId = elevDist(gen);
                    engine.requestCarCall(elevId, floor);
                }
                
                requestsMade++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(requestsMade.load(), numThreads * requestsPerThread);
    
    // Let simulation process remaining requests
    std::this_thread::sleep_for(std::chrono::seconds(1));
    engine.stop();
    
    SUCCEED();
}

// ============== EventQueue Thread Safety ==============

TEST(StressTest, EventQueueConcurrency) {
    EventQueue<int> queue;
    
    const int numProducers = 4;
    const int numConsumers = 2;
    const int itemsPerProducer = 100;
    
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    std::vector<std::thread> threads;
    
    // Producers
    for (int p = 0; p < numProducers; ++p) {
        threads.emplace_back([&queue, &produced, itemsPerProducer, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.push(p * itemsPerProducer + i);
                produced++;
            }
        });
    }
    
    // Consumers
    for (int c = 0; c < numConsumers; ++c) {
        threads.emplace_back([&queue, &consumed, numProducers, itemsPerProducer]() {
            int expected = numProducers * itemsPerProducer;
            while (consumed.load() < expected) {
                auto item = queue.tryPop();
                if (item.has_value()) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Wait for producers
    for (int i = 0; i < numProducers; ++i) {
        threads[i].join();
    }
    
    // Give consumers time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Signal shutdown for any blocked consumers
    queue.shutdown();
    
    // Wait for consumers
    for (int i = numProducers; i < numProducers + numConsumers; ++i) {
        threads[i].join();
    }
    
    EXPECT_EQ(produced.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(consumed.load(), numProducers * itemsPerProducer);
}

// ============== Long Running Test ==============

TEST(StressTest, Endurance) {
    Config config;
    config.numFloors = 10;
    config.numElevators = 3;
    config.tickDurationMs = 20;  // Fast ticks
    
    SimulationEngine engine(config);
    engine.start();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> floorDist(1, 10);
    
    // Run for ~500 ticks worth of time
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::seconds(10);
    
    int requestCount = 0;
    while (std::chrono::steady_clock::now() < endTime) {
        int floor = floorDist(gen);
        Direction dir = (floor <= 5) ? Direction::Up : Direction::Down;
        if (floor == 1) dir = Direction::Up;
        if (floor == 10) dir = Direction::Down;
        
        engine.requestHallCall(floor, dir);
        requestCount++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    engine.stop();
    
    std::cout << "Endurance test: " << requestCount << " requests over 10 seconds, "
              << engine.getCurrentTick() << " ticks processed.\n";
    
    EXPECT_GT(engine.getCurrentTick(), 100);  // Should have many ticks
    SUCCEED();
}

// ============== Rapid Start/Stop ==============

TEST(StressTest, RapidStartStop) {
    Config config;
    config.numFloors = 5;
    config.numElevators = 2;
    config.tickDurationMs = 100;
    
    for (int i = 0; i < 10; ++i) {
        SimulationEngine engine(config);
        engine.start();
        
        engine.requestHallCall(3, Direction::Up);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        engine.stop();
    }
    
    SUCCEED();
}

// ============== Main ==============

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
