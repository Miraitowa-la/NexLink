set(src_dirs
    KEY
    LED
    debug_gpio
)
set(include_dirs
    KEY
    LED
    debug_gpio
)
set(requires
    esp_driver_gpio
    hal
)

idf_component_register(
    SRC_DIRS
    ${src_dirs}
    INCLUDE_DIRS
    ${include_dirs}
    REQUIRES
    ${requires}
)
if(src_dirs)
    component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
endif()
