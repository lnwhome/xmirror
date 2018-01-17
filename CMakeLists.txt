cmake_minimum_required (VERSION 2.6)
project (vr_server)

set (CMAKE_CXX_FLAGS "-std=c++17 -U__STRICT_ANSI__ -g -Wall -Werror -Wextra -march=native -O3 -fomit-frame-pointer -fexpensive-optimizations -DGL_GLEXT_PROTOTYPES")

set (ZELEMENTS_SOURCE_DIR "./miniZelements")
include_directories ("${ZELEMENTS_SOURCE_DIR}/ZelementsPool/" "${ZELEMENTS_SOURCE_DIR}/ZiDSStub/ "${ZELEMENTS_SOURCE_DIR}/)

add_executable(server main OpenGlWrap OpenHmdWrap RenderingEngine XServerMirror LoadPng Log
                      ${CMAKE_CURRENT_SOURCE_DIR}/${ZELEMENTS_SOURCE_DIR}/ZiDSStub/Evt
                      ${CMAKE_CURRENT_SOURCE_DIR}/${ZELEMENTS_SOURCE_DIR}/ZelementsPool/CameraInput/CameraInput
                      ${CMAKE_CURRENT_SOURCE_DIR}/${ZELEMENTS_SOURCE_DIR}/ZelementsPool/CameraInput/hal/CameraInputv4l)

target_link_libraries (server pthread png GL X11 SDL2 openhmd GLEW glut Xi)

add_custom_command(
        TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/*.png
                ${CMAKE_CURRENT_BINARY_DIR})
