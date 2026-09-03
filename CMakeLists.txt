cmake_minimum_required(VERSION 3.20)
project(hydrogen_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib 5.5 REQUIRED)

add_library(quantum src/quantum.cpp)
target_include_directories(quantum PUBLIC src)
target_compile_options(quantum PRIVATE -Wall -Wextra -pedantic)

add_executable(atom src/main.cpp)
target_link_libraries(atom PRIVATE raylib quantum)
target_compile_options(atom PRIVATE -Wall -Wextra -pedantic)

include(CTest)

add_executable(quantum_test tests/quantum_test.cpp)
target_link_libraries(quantum_test PRIVATE quantum)
add_test(NAME quantum_test COMMAND quantum_test)
