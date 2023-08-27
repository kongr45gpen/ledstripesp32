FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/lib/json/single_include"
    EMBED_TXTFILES "${CMAKE_BINARY_DIR}/config.json"
)
component_compile_options(-Wno-error=format= -Wno-format)
