cmake_minimum_required(VERSION 3.20)

project(Pandora LANGUAGES C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_KEXT "Build the Pandora kernel extension" ON)
option(BUILD_LIBRARY "Build the Pandora user-space library and test" ON)

if(BUILD_KEXT)
    add_subdirectory(Kext)
endif()

if(BUILD_LIBRARY)
    add_subdirectory(Library)
endif()
