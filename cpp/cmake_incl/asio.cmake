# This is file to add ASIO to project.
# It is separated to keep main file smaller.

#https://github.com/cpm-cmake/CPM.cmake/blob/master/examples/asio-standalone/CMakeLists.txt

CPMAddPackage("gh:chriskohlhoff/asio#asio-1-36-0@1.36.0")

add_library(asio INTERFACE)
target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

