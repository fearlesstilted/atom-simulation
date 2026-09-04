cmake_minimum_required(VERSION 3.20)
project(hydrogen_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ATOM_SANITIZE "Enable address and undefined-behavior sanitizers" OFF)

find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

function(atom_target_defaults target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    if(ATOM_SANITIZE)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

add_library(quantum src/quantum.cpp)
target_include_directories(quantum PUBLIC src)
atom_target_defaults(quantum)

add_library(sampler src/sampler.cpp)
target_include_directories(sampler PUBLIC src)
target_link_libraries(sampler PUBLIC quantum)
atom_target_defaults(sampler)

add_library(state_sequence src/state_sequence.cpp)
target_include_directories(state_sequence PUBLIC src)
target_link_libraries(state_sequence PUBLIC quantum)
atom_target_defaults(state_sequence)

add_library(settings src/settings.cpp)
target_include_directories(settings PUBLIC src)
target_link_libraries(settings PUBLIC quantum)
atom_target_defaults(settings)

add_executable(atom src/main.cpp)
target_link_libraries(atom PRIVATE raylib quantum sampler state_sequence settings)
atom_target_defaults(atom)
target_compile_definitions(atom PRIVATE
    ATOM_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")

include(CTest)

add_executable(quantum_test tests/quantum_test.cpp)
target_link_libraries(quantum_test PRIVATE quantum)
atom_target_defaults(quantum_test)
add_test(NAME quantum_test COMMAND quantum_test)

add_executable(sampler_test tests/sampler_test.cpp)
target_link_libraries(sampler_test PRIVATE sampler)
atom_target_defaults(sampler_test)
add_test(NAME sampler_test COMMAND sampler_test)

add_executable(state_sequence_test tests/state_sequence_test.cpp)
target_link_libraries(state_sequence_test PRIVATE state_sequence)
atom_target_defaults(state_sequence_test)
add_test(NAME state_sequence_test COMMAND state_sequence_test)

add_executable(settings_test tests/settings_test.cpp)
target_link_libraries(settings_test PRIVATE settings)
atom_target_defaults(settings_test)
add_test(NAME settings_test COMMAND settings_test)
