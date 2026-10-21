cmake_minimum_required(VERSION 3.14)
project(PacketFragmentationSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(packet_sim_core STATIC
    SystemComponents.cpp
    SystemComponents.h
    Types.h
)
target_include_directories(packet_sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(packet_sim_core PUBLIC /W4 /permissive- /utf-8)
else()
    target_compile_options(packet_sim_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(packet_sim main.cpp)
target_link_libraries(packet_sim PRIVATE packet_sim_core)

enable_testing()
add_executable(packet_sim_tests tests.cpp)
target_link_libraries(packet_sim_tests PRIVATE packet_sim_core)
add_test(NAME packet_sim_tests COMMAND packet_sim_tests)
