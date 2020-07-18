cmake_minimum_required(VERSION 3.0)

project(ipup LANGUAGES C)

add_executable(ipup stable.c)

install(TARGETS ipup RUNTIME DESTINATION bin)

target_link_libraries(ipup curl json-c)
