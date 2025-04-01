cmake_minimum_required(VERSION 3.19)

project(mediumRare)

include(../CMake/CommonMacros.txt)

SETUP_APP(mediumRare "MediumRare")

add_dependencies(mediumRare CopyShaders)

target_link_libraries(mediumRare PRIVATE SharedUtils)
target_link_libraries(mediumRare PRIVATE "${CMAKE_SOURCE_DIR}/.build/deps/src/lightweightvk/third-party/deps/src/meshoptimizer/Release/meshoptimizer.lib")
