cmake_minimum_required(VERSION 3.19)

project(mediumRare)

include(../CMake/CommonMacros.txt)

SETUP_APP(mediumRare "MediumRare")

add_dependencies(mediumRare CopyShaders)

target_link_libraries(mediumRare PRIVATE SharedUtils)
