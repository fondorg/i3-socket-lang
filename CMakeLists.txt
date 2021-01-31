cmake_minimum_required(VERSION 3.17)
project(i3_socket_lang)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

include(FindX11)
add_executable(i3_socket_lang src/main.cpp)


add_subdirectory(i3ipc++)
include_directories(${I3IPCpp_INCLUDE_DIRS})
link_directories(${I3IPCpp_LIBRARY_DIRS})
target_link_libraries(i3_socket_lang ${I3IPCpp_LIBRARIES})

target_link_libraries(i3_socket_lang ${X11_LIBRARIES})
target_link_libraries(i3_socket_lang
        /usr/lib/x86_64-linux-gnu/libxkbfile.a)
