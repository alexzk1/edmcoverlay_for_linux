cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(edmc_linux_overlay LANGUAGES CXX)
include(cmake_incl/required_system_libraries.cmake)

include(cmake_incl/cpm_install.cmake)
include(cmake_incl/libraries.cmake)
include(cmake_incl/asio.cmake)

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
    asio_lib
    common
    emoji_renderer
    Threads::Threads
)

