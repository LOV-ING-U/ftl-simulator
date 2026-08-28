cmake_minimum_required(VERSION 4.3)
project(ftlsimulator)

set(CMAKE_CXX_STANDARD 20)

add_executable(ftlsimulator main.cpp
        types.h
        flash_memory.h
        flash_memory.cpp
        mapping_table_dram.h
        mapping_table_dram.cpp)
