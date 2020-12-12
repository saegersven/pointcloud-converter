cmake_minimum_required(VERSION 3.10)

project(pointcloud_converter)

include_directories("src")
add_executable(converter src/main.cpp)