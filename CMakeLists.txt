cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 启用构建时显示每一条执行的命令行
# set(CMAKE_VERBOSE_MAKEFILE ON)

project(live-rtmp-monitor)

if (MINGW)
    add_link_options(-Wl,--gc-sections -static-libstdc++ -static-libgcc -static -s)
endif()

write_file(${CMAKE_BINARY_DIR}/.gitignore "*")
write_file(${CMAKE_CURRENT_SOURCE_DIR}/wxWidgets-3.2.8/.gitignore "*")

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

if (MSVC)
    set_target_properties(live-rtmp-monitor PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

# 添加资源文件
target_sources(live-rtmp-monitor PRIVATE
    src/resource.rc
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
