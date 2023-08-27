FILE(GLOB_RECURSE esp32_addressable_sources ../../lib/esp32_addressable/*.c)

idf_component_register(
    SRCS ${esp32_addressable_sources}
    INCLUDE_DIRS "../../lib/esp32_addressable/inc"
    REQUIRES "driver"
)
