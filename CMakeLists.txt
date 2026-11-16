cmake_minimum_required(VERSION 3.25)

project(SkyrimMouseRelease VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# One DLL for Special Edition (1.5.97) and Anniversary Edition (1.6.x, 1.7.x) runtimes, via Address Library.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/CommonLibSSE-NG EXCLUDE_FROM_ALL)
# Source builds don't expose add_commonlibsse_plugin(); include the helper directly.
include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(${PROJECT_NAME}
    NAME "SkyrimMouseRelease"
    AUTHOR "ThatTedGuy"
    SOURCES
        src/main.cpp
        src/Settings.cpp
        src/MouseRelease.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)
# The generated plugin-info source relies on a precompiled header for std types and literals.
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(${PROJECT_NAME} PRIVATE user32)

# Collect the DLL and default INI into dist/ in the layout of a Vortex mod archive.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/config/SkyrimMouseRelease.ini" "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins/")
