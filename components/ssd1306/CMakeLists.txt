FILE(GLOB_RECURSE ssd1306_sources ../../lib/ssd1306/src/*.cpp)

idf_component_register(
    SRCS ${ssd1306_sources}
    INCLUDE_DIRS "../../lib/ssd1306/src"
    REQUIRES "driver"
)