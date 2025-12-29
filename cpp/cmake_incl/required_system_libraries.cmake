#Required packages, please install it using package manager, on Ubuntu it may have prefix "lib"
#or suffix "-dev" or both.

# Required by main function.
find_package(X11 COMPONENTS Xfixes Xext Xrender REQUIRED)

# Required by emoji_renderer.
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(Fontconfig REQUIRED)

# Generic multithreading support. It should never fail on desktop OSes.
# https://cmake.org/cmake/help/latest/module/FindThreads.html
find_package(Threads REQUIRED)
