cmake_minimum_required(VERSION 3.20)
project(Pandora LANGUAGES C CXX ASM)

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The macOS kernel headers (Kernel.framework) rely on AppleClang-specific
# builtins (e.g. __builtin_ptrauth_string_discriminator). Building with a
# non-Apple Clang toolchain (such as Homebrew LLVM) typically fails with errors
# like "statement expression not allowed at file scope" coming from ptrauth.h.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "Pandora kext must be built with AppleClang. "
        "Reconfigure with -DCMAKE_C_COMPILER=\"$(xcrun --find clang)\" "
        "-DCMAKE_CXX_COMPILER=\"$(xcrun --find clang++)\". "
        "Current: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID}).")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Architecture settings
set(CMAKE_OSX_ARCHITECTURES "arm64e")

# Get macOS SDK path
execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE MACOS_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Prevent CMake from automatically linking standard libraries
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES "")

# Compiler flags for kernel extension
set(KERNEL_CXX_FLAGS
    -Wall
    -O3
    -nostdinc
    # -nostdlib
    -mkernel
    -fno-use-cxa-atexit
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-c++-static-destructors
    -DKERNEL
    -isystem ${MACOS_SDK_PATH}/System/Library/Frameworks/Kernel.framework/Headers
)

set(KERNEL_LINK_FLAGS
    -Wl,-kext
    -lcc_kext
    -nostdlib++
    -nodefaultlibs
)

# Automatically discover source files
file(GLOB_RECURSE CXX_SOURCES "src/*.cpp")
file(GLOB_RECURSE ASM_SOURCES "src/*.S")
file(GLOB_RECURSE HEADERS "src/*.h")

# Filter out old/ directory if present
list(FILTER CXX_SOURCES EXCLUDE REGEX "src/old/.*")
list(FILTER ASM_SOURCES EXCLUDE REGEX "src/old/.*")
list(FILTER HEADERS EXCLUDE REGEX "src/old/.*")

# Extract version from Info.plist and generate Version.h
execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Print :CFBundleVersion" ${CMAKE_CURRENT_SOURCE_DIR}/misc/Info.plist
    OUTPUT_VARIABLE PANDORA_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h @ONLY
)
include_directories(${CMAKE_BINARY_DIR}/generated)

# Create the main executable target
add_executable(${PROJECT_NAME} ${CXX_SOURCES} ${ASM_SOURCES} ${HEADERS})

# Apply kernel-specific compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE ${KERNEL_CXX_FLAGS})
target_link_options(${PROJECT_NAME} PRIVATE ${KERNEL_LINK_FLAGS})

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext/Contents/MacOS"
)

# Create kext bundle directories
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext/Contents/MacOS"
)

# Copy Info.plist
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/misc/Info.plist"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext/Contents/Info.plist"
)

# Code signing
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND codesign -s - -f "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext"
    COMMENT "Code signing ${PROJECT_NAME}.kext"
)

# Build target (equivalent to 'make all')
add_custom_target(build ALL
    DEPENDS ${PROJECT_NAME}
)

# Install target
add_custom_target(install_kext
    COMMAND sudo chown -R root:wheel "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext"
    COMMAND sudo cp -r "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext" "/Library/Extensions"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Installing ${PROJECT_NAME}.kext to /Library/Extensions"
)

# Clean target (automatically handled by CMake, but we can add custom clean if needed)
set_property(DIRECTORY PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.kext"
)

# Print configuration information
list(LENGTH CXX_SOURCES CXX_COUNT)
list(LENGTH ASM_SOURCES ASM_COUNT)
list(LENGTH HEADERS HEADER_COUNT)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "macOS SDK Path: ${MACOS_SDK_PATH}")
message(STATUS "Architecture: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "Found ${CXX_COUNT} C++ source files")
message(STATUS "Found ${ASM_COUNT} assembly source files")
message(STATUS "Found ${HEADER_COUNT} header files")
