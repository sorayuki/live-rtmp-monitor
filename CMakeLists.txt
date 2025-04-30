cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(live-rtmp-monitor)

set(wxBUILD_SHARED OFF)
set(wxBUILD_MSVC_MULTIPROC OFF)
set(wxBUILD_MSVC_RUNTIME_STATIC ON)
add_subdirectory(wxWidgets-3.2.8 EXCLUDE_FROM_ALL)

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
    wx::core
    wx::base
)
set_target_properties(live-rtmp-monitor PROPERTIES
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)