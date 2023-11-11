project(smallchat-win32)
cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD_REQUIRED 99)
set(CMAKE_C_STANDARD 99)

add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_executable(smallchat-server chatlib.c smallchat-server.c)
target_link_libraries(smallchat-server ws2_32)

add_executable(smallchat-client chatlib.c smallchat-client.c)
target_link_libraries(smallchat-client ws2_32)
