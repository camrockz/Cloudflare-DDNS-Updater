cmake_minimum_required(VERSION 3.0)

project(ipup LANGUAGES C)

add_executable(ipup daemon.c)

install(TARGETS ipup RUNTIME DESTINATION bin)

option(SYSTEMD "Build with systemd support" ON)

if(SYSTEMD)
    add_compile_definitions(SYSTEMD)
    target_link_libraries(ipup curl json-c systemd)
else()
    target_link_libraries(ipup curl json-c)
endif()
