idf_component_register(
    SRCS "cvbs_main.cpp" 
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_psram esp_driver_gpio esp_driver_dma
)
