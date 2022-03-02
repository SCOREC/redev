cmake_minimum_required(VERSION 3.15.0...3.21.0)

project(foo VERSION 1.0.0 LANGUAGES CXX)

find_package(redev REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main redev::redev)
