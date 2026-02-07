cmake_minimum_required(VERSION 3.16)
project(ElevatorSimulation VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
add_compile_options(-Wall -Wextra -Werror -pedantic)

# Threading
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/Domain.cpp
    src/Scheduler.cpp
    src/Simulation.cpp
)

# Main library (for linking with tests)
add_library(elevator_lib STATIC ${SOURCES})
target_link_libraries(elevator_lib Threads::Threads)

# Main executable
add_executable(elevator src/main.cpp)
target_link_libraries(elevator elevator_lib)

# ==================== Testing ====================
enable_testing()

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)

    # Unit Tests
    add_executable(unit_tests tests/UnitTests.cpp)
    target_link_libraries(unit_tests elevator_lib GTest::gtest_main)
    
    # Stress Tests
    add_executable(stress_tests tests/StressTests.cpp)
    target_link_libraries(stress_tests elevator_lib GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(unit_tests)
    gtest_discover_tests(stress_tests)
endif()

# ==================== Sanitizers (Debug) ====================
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# ==================== Install ====================
install(TARGETS elevator DESTINATION bin)
