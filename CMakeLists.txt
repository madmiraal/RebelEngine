# Basic CMakeLists.txt for JetBrains IDEA users.
cmake_minimum_required(VERSION 4.0)
project(RebelEngine)

file(GLOB_RECURSE HEADERS **/*.h)
file(GLOB_RECURSE SOURCES **/*.cpp)
include_directories(.)
add_library(${PROJECT_NAME} STATIC ${SOURCES})
