cmake_minimum_required(VERSION 3.5.1)
project(HONS_Project C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)
FIND_LIBRARY(ZMQ_LIB zmq)
FIND_LIBRARY(CZMQ_LIB czmq)

# Hashmap implementation
add_library(HASHMAP_LIBRARY STATIC src/hashmap.c)

add_executable(client src/client.c)
TARGET_LINK_LIBRARIES(client ${ZMQ_LIB} ${CZMQ_LIB} HASHMAP_LIBRARY)

add_executable(orchestrator src/testOrchestrator.c)
TARGET_LINK_LIBRARIES(orchestrator ${ZMQ_LIB} ${CZMQ_LIB})
