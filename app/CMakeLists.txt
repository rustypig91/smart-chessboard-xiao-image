#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

# Get version from Git
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE APP_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

if(NOT APP_VERSION)
    set(APP_VERSION "unknown")
endif()

zephyr_compile_definitions("APP_VERSION_STRING=\"${APP_VERSION}\"")
target_sources(app PRIVATE src/main.c src/chessboard.c src/chessboard_cmd.c src/chessboard_calibration.c src/watchdog.c src/version_cmd.c)
