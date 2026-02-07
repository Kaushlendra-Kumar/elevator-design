#include "Simulation.hpp"
#include <iostream>
#include <cstring>

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n"
              << "\nOptions:\n"
              << "  -f, --floors <n>      Number of floors (1-12, default: 10)\n"
              << "  -e, --elevators <n>   Number of elevators (1-3, default: 3)\n"
              << "  -c, --capacity <n>    Car capacity (1-10, default: 6)\n"
              << "  -m, --mode <type>     Controller mode: master|distributed (default: master)\n"
              << "  -t, --tick <ms>       Tick duration in ms (100-2000, default: 500)\n"
              << "  -h, --help            Show this help\n"
              << "\nExample:\n"
              << "  " << progName << " -f 12 -e 3 -m distributed\n";
}

bool parseArgs(int argc, char* argv[], Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }
        else if ((arg == "-f" || arg == "--floors") && i + 1 < argc) {
            config.numFloors = std::stoi(argv[++i]);
            if (config.numFloors < 1 || config.numFloors > 12) {
                std::cerr << "Error: floors must be 1-12\n";
                return false;
            }
        }
        else if ((arg == "-e" || arg == "--elevators") && i + 1 < argc) {
            config.numElevators = std::stoi(argv[++i]);
            if (config.numElevators < 1 || config.numElevators > 3) {
                std::cerr << "Error: elevators must be 1-3\n";
                return false;
            }
        }
        else if ((arg == "-c" || arg == "--capacity") && i + 1 < argc) {
            config.carCapacity = std::stoi(argv[++i]);
            if (config.carCapacity < 1 || config.carCapacity > 10) {
                std::cerr << "Error: capacity must be 1-10\n";
                return false;
            }
        }
        else if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "master") {
                config.controllerType = ControllerType::Master;
            } else if (mode == "distributed") {
                config.controllerType = ControllerType::Distributed;
            } else {
                std::cerr << "Error: mode must be 'master' or 'distributed'\n";
                return false;
            }
        }
        else if ((arg == "-t" || arg == "--tick") && i + 1 < argc) {
            config.tickDurationMs = std::stoi(argv[++i]);
            if (config.tickDurationMs < 100 || config.tickDurationMs > 2000) {
                std::cerr << "Error: tick must be 100-2000 ms\n";
                return false;
            }
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    Config config;
    
    if (!parseArgs(argc, argv, config)) {
        return 1;
    }
    
    std::cout << "========================================\n"
              << "       Elevator Simulation System       \n"
              << "========================================\n"
              << "Configuration:\n"
              << "  Floors:     " << config.numFloors << "\n"
              << "  Elevators:  " << config.numElevators << "\n"
              << "  Capacity:   " << config.carCapacity << "\n"
              << "  Controller: " << (config.controllerType == ControllerType::Master 
                                      ? "Master" : "Distributed") << "\n"
              << "  Tick:       " << config.tickDurationMs << " ms\n"
              << "========================================\n";
    
    try {
        SimulationEngine engine(config);
        CLI cli(engine);
        
        engine.start();
        cli.run();
        engine.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "Simulation ended.\n";
    return 0;
}
