cmake_minimum_required(VERSION 3.20)
project(hydrogen_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib 5.5 REQUIRED)

add_library(quantum src/quantum.cpp)
target_include_directories(quantum PUBLIC src)
target_compile_options(quantum PRIVATE -Wall -Wextra -pedantic)

add_library(sampler src/sampler.cpp)
target_include_directories(sampler PUBLIC src)
target_link_libraries(sampler PUBLIC quantum)
target_compile_options(sampler PRIVATE -Wall -Wextra -pedantic)

add_library(state_sequence src/state_sequence.cpp)
target_include_directories(state_sequence PUBLIC src)
target_link_libraries(state_sequence PUBLIC quantum)
target_compile_options(state_sequence PRIVATE -Wall -Wextra -pedantic)

add_executable(atom src/main.cpp)
target_link_libraries(atom PRIVATE raylib quantum sampler state_sequence)
target_compile_options(atom PRIVATE -Wall -Wextra -pedantic)

include(CTest)

add_executable(quantum_test tests/quantum_test.cpp)
target_link_libraries(quantum_test PRIVATE quantum)
add_test(NAME quantum_test COMMAND quantum_test)

add_executable(sampler_test tests/sampler_test.cpp)
target_link_libraries(sampler_test PRIVATE sampler)
add_test(NAME sampler_test COMMAND sampler_test)

add_executable(state_sequence_test tests/state_sequence_test.cpp)
target_link_libraries(state_sequence_test PRIVATE state_sequence)
add_test(NAME state_sequence_test COMMAND state_sequence_test)
