set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(pcc main.cpp Utils.cpp Reader.cpp Builder.cpp BufferedPointReader.cpp BufferedPointWriter.cpp)