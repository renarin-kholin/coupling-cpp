cmake_minimum_required(VERSION 3.27)
project(coupling VERSION 0.1 LANGUAGES CXX C)

add_executable(coupling_linux src/main.cpp)

add_subdirectory(dependencies/datachannel)
target_link_libraries(coupling_linux datachannel)
target_include_directories(coupling_linux PUBLIC dependencies/datachannel/include)
