cmake_minimum_required(VERSION 3.20)
project(emoji_renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check required_system_libraries.cmake for find_package()

file(GLOB PROJECT_SOURCES
        *.h
        *.hpp
        *.cpp
    )

add_library(emoji_renderer STATIC ${PROJECT_SOURCES})

target_include_directories(emoji_renderer
    PUBLIC
        ${CMAKE_SOURCE_DIR}/common
        ./
)
target_include_directories(emoji_renderer PRIVATE ${FREETYPE_INCLUDE_DIRS} ${Fontconfig_INCLUDE_DIRS})
target_link_libraries(emoji_renderer PRIVATE ${FREETYPE_LIBRARIES}  ${Fontconfig_LIBRARIES} PNG::PNG)



