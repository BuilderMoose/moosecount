cmake_minimum_required(VERSION 3.16)
project(MooseCount VERSION 0.4.0)

# Force C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Optimization flags for Release mode
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")

# Aggressive warnings, applied to our targets only so googletest builds quietly
set(MOOSECOUNT_WARNINGS -Wall -Wextra -pedantic -Wformat-security)

# CI turns warnings into errors. Left off locally so a work-in-progress build
# is not blocked by an unused variable.
option(MOOSECOUNT_WERROR "Treat warnings as errors" OFF)
if(MOOSECOUNT_WERROR)
  list(APPEND MOOSECOUNT_WARNINGS -Werror)
endif()

# Unit test framework, vendored as a submodule
add_subdirectory(google)

# Everything except the command line lives in the library, so the tests can
# link against it rather than shelling out to the binary
add_library(MoosecountLib STATIC
  src/document.cpp
  src/ignore.cpp
  src/language.cpp
  src/options.cpp
  src/parser.cpp
  src/report.cpp
  src/walker.cpp
)
target_include_directories(MoosecountLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(MoosecountLib PRIVATE ${MOOSECOUNT_WARNINGS})

add_executable(moosecount main.cpp)
target_link_libraries(moosecount PRIVATE MoosecountLib)
target_compile_options(moosecount PRIVATE ${MOOSECOUNT_WARNINGS})

add_executable(moosemetrics metrics_main.cpp)
target_link_libraries(moosemetrics PRIVATE MoosecountLib)
target_compile_options(moosemetrics PRIVATE ${MOOSECOUNT_WARNINGS})

# So --version reports the number declared above, not one copied by hand
target_compile_definitions(MoosecountLib PUBLIC MOOSECOUNT_VERSION="${PROJECT_VERSION}")

enable_testing()
add_subdirectory(tests/unit)
