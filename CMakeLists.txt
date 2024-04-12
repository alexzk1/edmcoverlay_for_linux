cmake_minimum_required(VERSION 3.5)

project(edmc_overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(X11 COMPONENTS Xft Xfixes Xext REQUIRED)

add_executable(overlay overlay.cpp json.hpp socket.cc socket.hh json_message.cc json_message.hh xoverlayoutput.cpp xoverlayoutput.h
    opaque_ptr.h cm_ctors.h colors_mgr.h drawables.h layer_out.h runners.h)

target_include_directories(overlay PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    ${X11_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${Fontconfig_INCLUDE_DIR})

target_link_libraries(overlay PUBLIC ${X11_X11_LIB} ${X11_Xfixes_LIB}
    ${X11_Xft_LIB} ${X11_Xext_LIB}
    ${FREETYPE_LIBRARIES} ${Fontconfig_LIBRARY} -lpthread)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Wall -g -O0")
