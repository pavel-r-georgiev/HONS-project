# cmake_minimum_required(VERSION <specify CMake version here>)
project(HONS_Project C)

set(CMAKE_C_STANDARD 99)

add_library(HONS_Project src/heartbeatClient.c src/heartbeatServer.c)