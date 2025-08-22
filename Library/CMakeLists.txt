cmake_minimum_required(VERSION 3.20)

project(PandoraLibrary C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(pdtest
    pandora.c
    test.c
    pandora.h
)

target_compile_options(pdtest PRIVATE -Wall -Wextra)
target_include_directories(pdtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pdtest PRIVATE "-framework IOKit" "-framework CoreFoundation")

set_target_properties(pdtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build"
)

set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/entitlements.plist")

if(APPLE)
    if(EXISTS "${ENTITLEMENTS}")
        find_program(LDID_EXECUTABLE ldid)
        if(LDID_EXECUTABLE)
            add_custom_command(TARGET pdtest POST_BUILD
                COMMAND "${LDID_EXECUTABLE}" "-S${ENTITLEMENTS}" "$<TARGET_FILE:pdtest>"
                COMMENT "Code signing pdtest with ldid entitlements"
                VERBATIM
            )
        else()
            find_program(CODESIGN_EXECUTABLE codesign)
            if(CODESIGN_EXECUTABLE)
                add_custom_command(TARGET pdtest POST_BUILD
                    COMMAND "${CODESIGN_EXECUTABLE}" --force --sign - --entitlements "${ENTITLEMENTS}" "$<TARGET_FILE:pdtest>"
                    COMMENT "Code signing pdtest with codesign entitlements"
                    VERBATIM
                )
            endif()
        endif()
    endif()
endif()
