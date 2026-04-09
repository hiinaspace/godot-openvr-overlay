# Using the same minimum as the godot-cpp project
cmake_minimum_required(VERSION 3.17)

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "godot-openvr-overlay") # "The name of the library"
set(GODOT_PROJECT_DIR "project" CACHE STRING "The directory of a Godot project folder")

# Make sure all the dependencies are satisfied
find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

# Ensure godot-cpp submodule has been updated
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
    message(NOTICE "godot-cpp bindings source not found")
    message(NOTICE "initializing/updating the godot-cpp submodule...")

    # update the c++ bindings submodule to populate it with the necessary source for the library
    execute_process(
        COMMAND git submodule update --init godot-cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

# Use custom extension_api.json from our patched Godot build (has camera_set_override_projection)
set(GODOTCPP_CUSTOM_API_FILE "${CMAKE_CURRENT_LIST_DIR}/extension_api.json" CACHE FILEPATH "Custom extension API JSON")

add_subdirectory(godot-cpp SYSTEM)

# Add godot-cpp's module path and include the exported functions.
# This is made available for documentation generation
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

# The godot-cpp target has some of useful properties attached that can be retrieved like so.
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

# Now we can specify our own project which will inherit any global cmake properties or variables that have been defined.
project(godot-openvr-overlay
    VERSION 1.0
    DESCRIPTION "Godot GDExtension for rendering scenes as OpenVR projective overlays"
    LANGUAGES CXX
)

# OpenVR
set(OPENVR_DIR "S:/lib/openvr" CACHE PATH "Path to OpenVR SDK")
set(OPENVR_INCLUDE "${OPENVR_DIR}/headers")
set(OPENVR_LIB "${OPENVR_DIR}/lib/win64/openvr_api.lib")

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/openvr_overlay.cpp
    src/openvr_overlay.h
    src/openvr_math.h
)

target_include_directories(${LIBNAME} PRIVATE "${OPENVR_INCLUDE}")
target_link_libraries(${LIBNAME} PRIVATE godot-cpp "${OPENVR_LIB}")

# Require at least C++17 for this target
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)

set_target_properties(${LIBNAME}
    PROPERTIES
    # The generator expression here prevents msvc from adding a Debug or Release subdir.
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"

    PREFIX ""
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/bin/${GODOTCPP_PLATFORM}")

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GODOT_PROJECT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>" "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
    # Only copy openvr_api.dll if it doesn't already exist (avoids failure when Godot has it locked)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENVR_DIR}/bin/win64/openvr_api.dll" "${GODOT_PROJECT_BINARY_DIR}/openvr_api.dll"
)
