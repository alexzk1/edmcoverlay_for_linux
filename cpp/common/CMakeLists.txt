cmake_minimum_required(VERSION 3.20)
project(common LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB PROJECT_SOURCES
        *.h
        *.hpp
        *.cpp
    )

add_library(common STATIC ${PROJECT_SOURCES})
target_include_directories(common PUBLIC ./)
