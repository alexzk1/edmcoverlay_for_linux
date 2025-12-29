cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Download CPM.cmake (package manager for cmake)
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
  OPTIONS "JSON_BuildTests OFF"
)

CPMAddPackage(
  URI "gh:sammycage/lunasvg@3.5.0"
  OPTIONS "USE_SYSTEM_PLUTOVG OFF"
)

project(edmc_linux_overlay LANGUAGES CXX)

#Required packages, please install it using package manager, on Ubuntu it may have prefix "lib"
#or suffix "-dev" or both.

find_package(X11 COMPONENTS Xfixes Xext Xrender REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB PROJECT_SOURCES
        *.h
        *.hpp
        *.cpp
        *.cc
        *.hh
    )

add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -march=native -Wall)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${X11_INCLUDE_DIRS}
)

add_subdirectory(common)
add_subdirectory(emoji_renderer)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    ${X11_X11_LIB}
    ${X11_Xfixes_LIB}
    ${X11_Xext_LIB}
    ${X11_Xrender_LIB}
    nlohmann_json::nlohmann_json
    lunasvg::lunasvg
    common
    emoji_renderer
    -lpthread
)

