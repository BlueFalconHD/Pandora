cmake_minimum_required(VERSION 3.20)

project(PandoraLibrary C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE PANDORA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable(pdtest ${PANDORA_SOURCES})

target_compile_options(pdtest PRIVATE -Wall -Wextra)
target_include_directories(pdtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pdtest PRIVATE "-framework IOKit" "-framework CoreFoundation")

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CAPSTONE QUIET IMPORTED_TARGET capstone)
    if(TARGET PkgConfig::CAPSTONE)
        target_link_libraries(pdtest PRIVATE PkgConfig::CAPSTONE)
    endif()
endif()

if(NOT TARGET PkgConfig::CAPSTONE)
    find_library(CAPSTONE_LIBRARY NAMES capstone)
    if(CAPSTONE_LIBRARY)
        target_link_libraries(pdtest PRIVATE "${CAPSTONE_LIBRARY}")
    else()
        message(WARNING "Capstone library not found. Install via Homebrew: brew install capstone, or provide CAPSTONE_LIBRARY.")
    endif()
endif()

set_target_properties(pdtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/entitlements.plist")

if(APPLE)
    if(EXISTS "${ENTITLEMENTS}")
        # On macOS, prefer `codesign`. `ldid` may exist (via Homebrew) but does
        # not produce a code signature the OS will honor for entitlement checks.
        find_program(CODESIGN_EXECUTABLE codesign)
        if(CODESIGN_EXECUTABLE)
            add_custom_command(TARGET pdtest POST_BUILD
                COMMAND "${CODESIGN_EXECUTABLE}" --force --sign - --entitlements "${ENTITLEMENTS}" "$<TARGET_FILE:pdtest>"
                COMMENT "Code signing pdtest with codesign entitlements"
                VERBATIM
            )
        else()
            find_program(LDID_EXECUTABLE ldid)
            if(LDID_EXECUTABLE)
                add_custom_command(TARGET pdtest POST_BUILD
                    COMMAND "${LDID_EXECUTABLE}" "-S${ENTITLEMENTS}" "$<TARGET_FILE:pdtest>"
                    COMMENT "Code signing pdtest with ldid entitlements"
                    VERBATIM
                )
            endif()
        endif()
    endif()
endif()
