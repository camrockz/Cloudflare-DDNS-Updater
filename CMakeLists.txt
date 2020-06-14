cmake_minimum_required(VERSION 3.0)

project(ipup LANGUAGES C)

add_executable(ipup main.c)

install(TARGETS ipup RUNTIME DESTINATION bin)
