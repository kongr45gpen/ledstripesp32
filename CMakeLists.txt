cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 20)

include("${CMAKE_BINARY_DIR}/config/sdkconfig.cmake")

if (NOT DEFINED CONFIG_CONFIG_PATH)
    message(FATAL_ERROR "CONFIG_CONFIG_PATH is not defined")
elseif(CONFIG_CONFIG_PATH MATCHES "^/")
    set(CONFIG_FILE_ACTUAL_PATH ${CONFIG_CONFIG_PATH})
else()
    set(CONFIG_FILE_ACTUAL_PATH "${CMAKE_SOURCE_DIR}/${CONFIG_CONFIG_PATH}")
endif()

file(REMOVE "${CMAKE_BINARY_DIR}/config.json")
configure_file(${CONFIG_FILE_ACTUAL_PATH} "${CMAKE_BINARY_DIR}/config.json" COPYONLY)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ledstripesp32)
