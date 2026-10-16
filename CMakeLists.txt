cmake_minimum_required(VERSION 3.16)
project(kis_time_corridors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(kis-time-corridors
    src/main.cpp
    src/solver.cpp
)

target_compile_options(kis-time-corridors PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Автоматические тесты
enable_testing()

add_executable(kis-time-corridors-tests
    tests/tests.cpp
    src/solver.cpp
)

target_include_directories(kis-time-corridors-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(kis-time-corridors-tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_test(
    NAME solver-tests
    COMMAND kis-time-corridors-tests
)
