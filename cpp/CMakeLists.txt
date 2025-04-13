cmake_minimum_required(VERSION 3.20)

project(edmc_linux_overlay LANGUAGES CXX)

#Required packages, please install it using package manager, on Ubuntu it may have prefix "lib"
#or suffix "-dev" or both.

option(WITH_CAIRO "Build with Cairo support" OFF)

find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(X11 COMPONENTS Xft Xfixes Xext REQUIRED)

if(WITH_CAIRO)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)

    message(STATUS "Cairo include dirs: ${CAIRO_INCLUDE_DIRS}")
    message(STATUS "Cairo libraries: ${CAIRO_LIBRARIES}")

    add_definitions(-DWITH_CAIRO)

    include_directories(${CAIRO_INCLUDE_DIRS})
    link_directories(${CAIRO_LIBRARY_DIRS})
else()
    message(STATUS "Cairo support disabled.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(PROJECT_SOURCES
    main.cpp
    colors_mgr.h
    drawables.h
    layer_out.h
    xoverlayoutput.cpp xoverlayoutput.h
    json_message.cc json_message.hh
    socket.cc socket.hh

    #Common helpers
    json.hpp
    cm_ctors.h
    opaque_ptr.h
    runners.h
    strutils.h
)

add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -march=native -Wall)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    ${X11_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${Fontconfig_INCLUDE_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${X11_X11_LIB} ${X11_Xfixes_LIB}
    ${X11_Xft_LIB} ${X11_Xext_LIB} ${CAIRO_LIBRARIES}
    ${FREETYPE_LIBRARIES} ${Fontconfig_LIBRARY} -lpthread)

