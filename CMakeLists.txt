cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(live-rtmp-monitor)

add_executable(live-rtmp-monitor)
target_sources(live-rtmp-monitor PRIVATE
    src/main.cpp
    src/TCPConnection.h
    src/TCPConnection.cpp
)
set_target_properties(live-rtmp-monitor PROPERTIES
    WIN32_EXECUTABLE ON
)
target_link_libraries(live-rtmp-monitor
PRIVATE
    Ws2_32
    Iphlpapi
)
