# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include")
component_compile_options(-Wno-error=format= -Wno-format)