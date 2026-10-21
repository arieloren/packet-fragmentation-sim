cmake_minimum_required(VERSION 3.14)
project(PacketFragmentationSim CXX)

# C++17 must be specified, as we used std::optional
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Defining the executable filename and bundling all project files
add_executable(packet_sim
    main.cpp
    SystemComponents.cpp
    SystemComponents.h
    Types.h
)