cmake_minimum_required(VERSION 3.15)
project(test_project CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(test_prog main.cc utils.cc mock_v8.cc)
