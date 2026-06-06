set(srcs "src/u8g2_esp32_hal.c")
set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi driver freertos u8g2)