cmake_minimum_required(VERSION 3.5.1)
project(HONS_Project C CXX)

include_directories(src/include)
set(CMAKE_C_STANDARD 99)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
FIND_LIBRARY(ZMQ_LIB zmq)
FIND_LIBRARY(CZMQ_LIB czmq)
set(CMAKE_USE_PTHREADS_INIT true)
set(CMAKE_USE_PTHREADS_INIT ON)
#set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/lib/libpaxos/cmake")

find_package(Threads REQUIRED)
find_package (ZLOG REQUIRED)

set(SOURCE_FILES_CLIENT
        src/client.c
        src/paxos_replica.c
        src/utils.c
        src/timer.c
        src/tpl.c  # Serialization library
        src/adaptive_delay_model.c)

# PAXOS library
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpaxos/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpaxos/evpaxos/include)
include_directories(${LIBEVENT_INCLUDE_DIRS} ${MSGPACK_INCLUDE_DIRS})
link_directories(src/lib/libpaxos)
find_package(LIBEVENT REQUIRED)
find_package(MSGPACK REQUIRED)
find_package(LMDB)

if (LMDB_FOUND)
    add_definitions(-DHAS_LMDB)
endif()

if (NOT APPLE)
    set(LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} m rt)
endif()

file(GLOB EVPAXOS_SOURCES "src/lib/libpaxos/evpaxos/*.c")
add_library(EVPAXOS SHARED ${EVPAXOS_SOURCES})
file(GLOB PAXOS_SOURCES "src/lib/libpaxos/paxos/*.c")
add_library(PAXOS SHARED ${PAXOS_SOURCES})

# Link libraries to the client
add_executable(client ${SOURCE_FILES_CLIENT})
TARGET_LINK_LIBRARIES(client ${ZMQ_LIB} ${CZMQ_LIB} EVPAXOS PAXOS event_pthreads ${LIBPAXOS_LINKER_LIBS}
        ${LIBEVENT_LIBRARIES} ${MSGPACK_LIBRARIES} zlog Threads::Threads)

IF (LMDB_FOUND)
    INCLUDE_DIRECTORIES(${LMDB_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(client ${LMDB_LIBRARIES})
ENDIF ()

add_executable(orchestrator src/test_orchestrator.c)
TARGET_LINK_LIBRARIES(orchestrator ${ZMQ_LIB} ${CZMQ_LIB})
