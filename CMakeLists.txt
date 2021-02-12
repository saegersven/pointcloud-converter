cmake_minimum_required(VERSION 3.10)
project(PointCloudConverter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR})
add_executable(${PROJECT_NAME}
src/main.cpp src/Utils.cpp src/ThreadPool.cpp src/RawPointReader.cpp src/Logger.cpp src/LasPointReader.cpp src/Builder.cpp src/AsyncOctreeWriter.cpp)