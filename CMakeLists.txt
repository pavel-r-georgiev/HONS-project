cmake_minimum_required(VERSION 3.5.1)
project(HONS_Project C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)
FIND_LIBRARY(ZMQ_LIB zmq)
FIND_LIBRARY(CZMQ_LIB czmq)
set(CMAKE_USE_PTHREADS_INIT true)
set(CMAKE_USE_PTHREADS_INIT ON)

find_package(Threads REQUIRED)

# Hashmap implementation
add_library(TIMER_LIBRARY STATIC src/timer.c)

add_executable(client src/client.c)
TARGET_LINK_LIBRARIES(client ${ZMQ_LIB} ${CZMQ_LIB} TIMER_LIBRARY Threads::Threads)

add_executable(orchestrator src/test_orchestrator.c)
TARGET_LINK_LIBRARIES(orchestrator ${ZMQ_LIB} ${CZMQ_LIB})
