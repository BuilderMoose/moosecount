cmake_minimum_required(VERSION 3.10)
project(MooseCount VERSION 0.2.0)

# Force C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Enable aggressive compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wformat-security")

# Optimization flags for Release mode
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -flto")

# Renamed the executable
add_executable(moosecount main.cpp)

# So --version reports the number declared above, not one copied by hand
target_compile_definitions(moosecount PRIVATE MOOSECOUNT_VERSION="${PROJECT_VERSION}")