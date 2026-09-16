cmake_minimum_required(VERSION 3.16)
project(MooseCount VERSION 0.3.0)

# Force C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Optimization flags for Release mode
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")

# Aggressive warnings, applied to our targets only so googletest builds quietly
set(MOOSECOUNT_WARNINGS -Wall -Wextra -pedantic -Wformat-security)

# Unit test framework, vendored as a submodule
add_subdirectory(google)

# Everything except the command line lives in the library, so the tests can
# link against it rather than shelling out to the binary
add_library(MoosecountLib STATIC
  src/ignore.cpp
  src/language.cpp
  src/parser.cpp
  src/walker.cpp
)
target_include_directories(MoosecountLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(MoosecountLib PRIVATE ${MOOSECOUNT_WARNINGS})

add_executable(moosecount main.cpp)
target_link_libraries(moosecount PRIVATE MoosecountLib)
target_compile_options(moosecount PRIVATE ${MOOSECOUNT_WARNINGS})

# So --version reports the number declared above, not one copied by hand
target_compile_definitions(moosecount PRIVATE MOOSECOUNT_VERSION="${PROJECT_VERSION}")

enable_testing()
add_subdirectory(tests/unit)
