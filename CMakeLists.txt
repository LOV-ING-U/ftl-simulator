cmake_minimum_required(VERSION 4.3)
project(ftlsimulator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(ftlsimulator main.cpp
        types.h
        flash_memory.h
        flash_memory.cpp
        mapping_table_dram.h
        mapping_table_dram.cpp
        ftl_controller.h
        ftl_controller.cpp)
