cmake_minimum_required(VERSION 3.14)
project(PacketFragmentationSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(packet_sim_core STATIC
    src/SystemComponents.cpp
    include/SystemComponents.h
    include/Types.h
)
target_include_directories(packet_sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(packet_sim_core PUBLIC /W4 /permissive- /utf-8)
else()
    target_compile_options(packet_sim_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(packet_sim src/main.cpp)
target_link_libraries(packet_sim PRIVATE packet_sim_core)

