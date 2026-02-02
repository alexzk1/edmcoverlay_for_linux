# This is file to add ASIO to project.
# It is separated to keep main file smaller.

#https://github.com/cpm-cmake/CPM.cmake/blob/master/examples/asio-standalone/CMakeLists.txt

#Version 1-30-2 is most tested and do not use C++20 yet.
CPMAddPackage("gh:chriskohlhoff/asio#asio-1-30-2@1.30.2")

add_library(asio_lib INTERFACE)
if(asio_ADDED)
   target_include_directories(asio_lib SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
   target_compile_definitions(asio_lib INTERFACE
       ASIO_STANDALONE
       ASIO_NO_DEPRECATED
   )
   target_link_libraries(asio_lib INTERFACE Threads::Threads)
endif()
